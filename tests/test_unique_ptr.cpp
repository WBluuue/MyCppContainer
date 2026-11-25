#include <mycppcontainer/unique_ptr.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

struct LifetimeCounts {
    int constructed = 0;
    int live = 0;
    int destroyed = 0;
};

// 被管理对象不可默认构造、不可拷贝或移动：接管指针不应重新构造 T。
// 计数器比所有者活得更久，测试之间不共享静态计数状态。
struct TrackedObject {
    LifetimeCounts& counts;
    int value;

    TrackedObject(LifetimeCounts& counters, int initial_value)
        : counts(counters), value(initial_value)
    {
        ++counts.constructed;
        ++counts.live;
    }
    TrackedObject(const TrackedObject&) = delete;
    TrackedObject& operator=(const TrackedObject&) = delete;
    TrackedObject(TrackedObject&&) = delete;
    TrackedObject& operator=(TrackedObject&&) = delete;

    ~TrackedObject() noexcept
    {
        --counts.live;
        ++counts.destroyed;
    }
};

using IntOwner = mycppcontainer::UniquePtr<int>;
using ObjectOwner = mycppcontainer::UniquePtr<TrackedObject>;
using ConstIntOwner = mycppcontainer::UniquePtr<const int>;

// 这些断言检查接口契约；当前只有头文件骨架，尚未实现时预期无法编译。
// 不检查 UniquePtr 的移动能力，避免提前要求后续任务的接口。
static_assert(std::is_nothrow_default_constructible_v<IntOwner>);
static_assert(std::is_nothrow_constructible_v<IntOwner, int*>);
static_assert(std::is_nothrow_destructible_v<IntOwner>);
static_assert(!std::is_convertible_v<int*, IntOwner>);
static_assert(!std::is_copy_constructible_v<IntOwner>);
static_assert(!std::is_copy_assignable_v<IntOwner>);
static_assert(!std::is_constructible_v<IntOwner, IntOwner&>);
static_assert(!std::is_assignable_v<IntOwner&, IntOwner&>);

// const 修饰所有者，不自动把 T 变为 const T；观察接口不转移所有权。
static_assert(std::is_same_v<decltype(std::declval<const IntOwner&>().get()), int*>);
static_assert(std::is_same_v<decltype(*std::declval<const IntOwner&>()), int&>);
static_assert(std::is_same_v<decltype(std::declval<const IntOwner&>().operator->()), int*>);
static_assert(std::is_same_v<decltype(std::declval<const ConstIntOwner&>().get()), const int*>);
static_assert(std::is_same_v<decltype(*std::declval<const ConstIntOwner&>()), const int&>);
static_assert(std::is_same_v<decltype(std::declval<const ConstIntOwner&>().operator->()), const int*>);
static_assert(noexcept(std::declval<const IntOwner&>().get()));
static_assert(noexcept(*std::declval<const IntOwner&>()));
static_assert(noexcept(std::declval<const IntOwner&>().operator->()));

// explicit bool 支持 if、! 和显式转换，但不能隐式转换为 bool 或整数。
static_assert(std::is_constructible_v<bool, const IntOwner&>);
static_assert(!std::is_convertible_v<const IntOwner&, bool>);
static_assert(!std::is_convertible_v<IntOwner&, bool>);
static_assert(!std::is_convertible_v<const IntOwner&, int>);
static_assert(std::is_same_v<decltype(std::declval<const IntOwner&>().operator bool()), bool>);
static_assert(noexcept(static_cast<bool>(std::declval<const IntOwner&>())));

} // namespace

TEST_CASE("UniquePtr default construction is empty", "[unique_ptr][basic]")
{
    const ObjectOwner owner;
    CHECK(owner.get() == nullptr);
    CHECK_FALSE(static_cast<bool>(owner));
    CHECK(!owner);

    bool entered = false;
    if (owner) { entered = true; }
    CHECK_FALSE(entered);
    // 空所有者可以观察和析构，但不调用 * 或 -> 访问对象。
}

TEST_CASE("UniquePtr accepts a null pointer without creating an object",
          "[unique_ptr][basic]")
{
    ObjectOwner owner(nullptr);
    CHECK(owner.get() == nullptr);
    CHECK_FALSE(static_cast<bool>(owner));
    CHECK(!owner);
    // 不需要额外的 nullptr_t 构造重载；T* 参数可以接收 nullptr。
}

TEST_CASE("UniquePtr adopts one object and destroys it once",
          "[unique_ptr][basic][lifetime]")
{
    LifetimeCounts counts;
    {
        auto* raw = new TrackedObject(counts, 7);
        ObjectOwner owner(raw);
        // 构造只接管同一对象；raw 此后只是借用指针，不能手动 delete。
        CHECK(owner.get() == raw);
        CHECK(counts.constructed == 1);
        CHECK(counts.live == 1);
        CHECK(counts.destroyed == 0);
    }
    CHECK(counts.constructed == 1);
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
    // 生命周期计数验证析构；原始存储泄漏还需 Sanitizer 补充检查。
}

TEST_CASE("UniquePtr observers access the same owned object",
          "[unique_ptr][basic][access]")
{
    LifetimeCounts counts;
    {
        ObjectOwner owner(new TrackedObject(counts, 7));
        auto* borrowed = owner.get();
        REQUIRE(borrowed != nullptr);
        CHECK(std::addressof(*owner) == borrowed);
        CHECK(owner.operator->() == borrowed);
        CHECK(owner->value == 7);

        (*owner).value = 11;
        CHECK(owner->value == 11);
        owner->value = 13;
        CHECK(borrowed->value == 13);
        CHECK(owner.get() == borrowed); // 重复 get 不应清空所有者或转移所有权。
        CHECK(counts.constructed == 1);
        CHECK(counts.live == 1);
        CHECK(counts.destroyed == 0);
    }
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr bool tests the pointer rather than the pointee value",
          "[unique_ptr][basic][access]")
{
    IntOwner owner(new int(0));
    REQUIRE(owner.get() != nullptr);
    CHECK(*owner == 0);
    CHECK(static_cast<bool>(owner));
    CHECK_FALSE(!owner);

    bool entered = false;
    if (owner) { entered = true; }
    CHECK(entered);
}

TEST_CASE("UniquePtr const ownership preserves pointee constness",
          "[unique_ptr][basic][access]")
{
    // const UniquePtr<int> 类似 int* const，仍可修改所指向的 int。
    const IntOwner owner(new int(7));
    *owner = 11;
    CHECK(*owner == 11);
    CHECK(owner.get() == std::addressof(*owner));
    CHECK(owner.operator->() == owner.get());

    // UniquePtr<const int> 才暴露只读的 int；类型断言防止错误地去掉 const。
    ConstIntOwner readonly(new const int(13));
    CHECK(*readonly == 13);
    CHECK(readonly.get() == std::addressof(*readonly));
}

TEST_CASE("UniquePtr independent owners release only their own objects",
          "[unique_ptr][basic][lifetime]")
{
    LifetimeCounts counts;
    {
        ObjectOwner outer(new TrackedObject(counts, 7));
        {
            ObjectOwner inner(new TrackedObject(counts, 11));
            CHECK(outer.get() != inner.get());
            CHECK(counts.live == 2);
            CHECK(counts.destroyed == 0);
        }
        // 内层先析构，不影响外层所拥有的对象。
        CHECK(counts.live == 1);
        CHECK(counts.destroyed == 1);
        CHECK(outer->value == 7);
    }
    CHECK(counts.constructed == 2);
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 2);
}

TEST_CASE("UniquePtr destroys its object during stack unwinding",
          "[unique_ptr][basic][lifetime][exception]")
{
    LifetimeCounts counts;
    const auto leave_with_exception = [&] {
        ObjectOwner owner(new TrackedObject(counts, 7));
        // 模拟接管资源之后的业务异常；不是要求 deleter 或析构函数抛异常。
        throw std::runtime_error("leave owning scope");
    };
    REQUIRE_THROWS_AS(leave_with_exception(), std::runtime_error);
    CHECK(counts.constructed == 1);
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}
