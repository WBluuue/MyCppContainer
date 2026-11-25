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

struct ResetObservation {
    bool destructor_called = false;
    bool saw_expected_pointer = false;
};

// 旧对象的析构可以观察 owner.get()；reset 应先公布新指针，再删除旧对象。
// 这里只读取所有者，不在析构中重入 reset 或修改它。
struct ResetObserver {
    mycppcontainer::UniquePtr<ResetObserver>* owner = nullptr;
    ResetObserver* expected = nullptr;
    ResetObservation* observation = nullptr;

    ~ResetObserver() noexcept
    {
        if (observation != nullptr)
        {
            observation->destructor_called = true;
            observation->saw_expected_pointer = (owner->get() == expected);
        }
    }
};

// U4a 只覆盖可默认构造、可拷贝的值类型删除器；move-only 和布局压缩留待后续。
struct PlainDelete {
    void operator()(TrackedObject* ptr) const noexcept { delete ptr; }
};

struct DeletionCounts {
    int calls = 0;
    int null_calls = 0;
};

// 两个实例可以绑定不同计数器，用来发现“指针转移了，删除策略却没转移”。
// 所有状态属于当前测试；默认状态也可析构，空指针调用可被单独记录。
struct CountingDeleter {
    DeletionCounts* counts = nullptr;

    CountingDeleter() noexcept = default;
    explicit CountingDeleter(DeletionCounts& counters) noexcept : counts(&counters) {}
    CountingDeleter(const CountingDeleter&) noexcept = default;
    CountingDeleter& operator=(const CountingDeleter&) noexcept = default;

    CountingDeleter(CountingDeleter&& other) noexcept
        : counts(std::exchange(other.counts, nullptr)) {}

    CountingDeleter& operator=(CountingDeleter&& other) noexcept
    {
        counts = std::exchange(other.counts, nullptr);
        return *this;
    }

    void operator()(TrackedObject* ptr) noexcept
    {
        if (counts != nullptr)
        {
            ++counts->calls;
            if (ptr == nullptr) { ++counts->null_calls; }
        }
        delete ptr;
    }
};

using PlainOwner = mycppcontainer::UniquePtr<TrackedObject, PlainDelete>;
using CustomOwner = mycppcontainer::UniquePtr<TrackedObject, CountingDeleter>;

// 已通过的基础接口仍是回归要求；新增删除策略不能恢复任何拷贝通道。
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

// 同类型所有权移动已验收；被管理的 T 本身可以完全不可移动。
static_assert(std::is_nothrow_move_constructible_v<IntOwner>);
static_assert(std::is_nothrow_move_assignable_v<IntOwner>);
static_assert(std::is_nothrow_move_constructible_v<ObjectOwner>);
static_assert(std::is_nothrow_move_assignable_v<ObjectOwner>);
static_assert(std::is_nothrow_move_constructible_v<ConstIntOwner>);
static_assert(std::is_nothrow_move_assignable_v<ConstIntOwner>);
static_assert(std::is_same_v<decltype(std::declval<ObjectOwner&>() =
                                    std::declval<ObjectOwner&&>()), ObjectOwner&>);
// std::move 不会去掉 const；不能从一个无法清空的 const 所有者偷取资源。
static_assert(!std::is_constructible_v<IntOwner, const IntOwner&&>);
static_assert(!std::is_assignable_v<IntOwner&, const IntOwner&&>);

// release/reset/swap 已验收；加入删除策略后仍需保持这些接口和 noexcept。
static_assert(std::is_same_v<decltype(std::declval<IntOwner&>().release()), int*>);
static_assert(std::is_same_v<decltype(std::declval<ConstIntOwner&>().release()), const int*>);
static_assert(noexcept(std::declval<IntOwner&>().release()));
static_assert(std::is_same_v<decltype(std::declval<IntOwner&>().reset()), void>);
static_assert(std::is_same_v<decltype(std::declval<IntOwner&>().reset(nullptr)), void>);
static_assert(std::is_same_v<decltype(std::declval<IntOwner&>().reset(
                                    std::declval<int*>())), void>);
static_assert(std::is_same_v<decltype(std::declval<ConstIntOwner&>().reset(
                                    std::declval<const int*>())), void>);
static_assert(noexcept(std::declval<IntOwner&>().reset()));
static_assert(noexcept(std::declval<IntOwner&>().reset(std::declval<int*>())));
static_assert(std::is_same_v<decltype(std::declval<ObjectOwner&>().swap(
                                    std::declval<ObjectOwner&>())), void>);
static_assert(noexcept(std::declval<ObjectOwner&>().swap(std::declval<ObjectOwner&>())));

// U4a 新接口；当前单模板参数版本尚未实现，暂时无法编译属于预期状态。
static_assert(std::is_empty_v<PlainDelete>);
static_assert(std::is_same_v<IntOwner,
                           mycppcontainer::UniquePtr<int, std::default_delete<int>>>);
static_assert(std::is_nothrow_default_constructible_v<PlainOwner>);
static_assert(std::is_nothrow_constructible_v<PlainOwner, TrackedObject*>);
static_assert(std::is_nothrow_default_constructible_v<CustomOwner>);
static_assert(std::is_nothrow_constructible_v<CustomOwner, TrackedObject*,
                                            const CountingDeleter&>);
static_assert(std::is_nothrow_constructible_v<CustomOwner, TrackedObject*,
                                            CountingDeleter&&>);
static_assert(std::is_nothrow_destructible_v<CustomOwner>);
static_assert(!std::is_copy_constructible_v<CustomOwner>);
static_assert(!std::is_copy_assignable_v<CustomOwner>);
static_assert(std::is_nothrow_move_constructible_v<CustomOwner>);
static_assert(std::is_nothrow_move_assignable_v<CustomOwner>);
static_assert(std::is_same_v<decltype(std::declval<CustomOwner&>().get_deleter()),
                           CountingDeleter&>);
static_assert(std::is_same_v<decltype(std::declval<const CustomOwner&>().get_deleter()),
                           const CountingDeleter&>);
static_assert(std::is_same_v<decltype(std::declval<ObjectOwner&>().get_deleter()),
                           std::default_delete<TrackedObject>&>);
static_assert(noexcept(std::declval<CustomOwner&>().get_deleter()));
static_assert(noexcept(std::declval<const CustomOwner&>().get_deleter()));
static_assert(noexcept(std::declval<CustomOwner&>().reset()));
static_assert(noexcept(std::declval<CustomOwner&>().release()));
static_assert(noexcept(std::declval<CustomOwner&>().swap(std::declval<CustomOwner&>())));

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

TEST_CASE("UniquePtr move construction from empty leaves both owners empty",
          "[unique_ptr][move]")
{
    ObjectOwner source;
    ObjectOwner destination(std::move(source));
    CHECK(source.get() == nullptr);
    CHECK(destination.get() == nullptr);
    CHECK_FALSE(static_cast<bool>(source));
    CHECK_FALSE(static_cast<bool>(destination));
}

TEST_CASE("UniquePtr move construction transfers ownership without moving the object",
          "[unique_ptr][move][lifetime]")
{
    LifetimeCounts counts;
    {
        ObjectOwner source(new TrackedObject(counts, 7));
        auto* object = source.get();
        {
            ObjectOwner destination(std::move(source));
            CHECK(source.get() == nullptr);
            CHECK_FALSE(static_cast<bool>(source));
            REQUIRE(destination.get() == object);
            REQUIRE(counts.live == 1);
            CHECK(destination->value == 7);
            CHECK(static_cast<bool>(destination));
            CHECK(counts.constructed == 1);
            CHECK(counts.destroyed == 0);
        }
        // 目标先析构并删除对象；源仍存活，但不再拥有任何资源。
        CHECK(source.get() == nullptr);
        CHECK(counts.live == 0);
        CHECK(counts.destroyed == 1);
    }
    CHECK(counts.constructed == 1);
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr move-constructed destination outlives its source",
          "[unique_ptr][move][lifetime]")
{
    LifetimeCounts counts;
    {
        auto destination = [&counts] {
            ObjectOwner source(new TrackedObject(counts, 7));
            // 显式从 source 构造返回对象；源先析构，不能删除已交出的对象。
            return ObjectOwner(std::move(source));
        }();
        REQUIRE(destination.get() != nullptr);
        REQUIRE(counts.live == 1);
        CHECK(destination->value == 7);
        CHECK(counts.constructed == 1);
        CHECK(counts.destroyed == 0);
    }
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr move assignment handles every empty and owning combination",
          "[unique_ptr][move_assignment][lifetime]")
{
    bool source_owns = false;
    bool destination_owns = false;
    SECTION("both empty") {}
    SECTION("owning source and empty destination") { source_owns = true; }
    SECTION("empty source and owning destination") { destination_owns = true; }
    SECTION("both owning")
    {
        source_owns = true;
        destination_owns = true;
    }

    LifetimeCounts source_counts;
    LifetimeCounts destination_counts;
    {
        ObjectOwner source(source_owns ? new TrackedObject(source_counts, 7) : nullptr);
        ObjectOwner destination(destination_owns ?
                                   new TrackedObject(destination_counts, 11) : nullptr);
        auto* transferred = source.get();
        auto& result = (destination = std::move(source));

        CHECK(std::addressof(result) == std::addressof(destination));
        CHECK(source.get() == nullptr);
        CHECK_FALSE(static_cast<bool>(source));
        REQUIRE(destination.get() == transferred);
        CHECK(static_cast<bool>(destination) == source_owns);
        CHECK(source_counts.constructed == (source_owns ? 1 : 0));
        REQUIRE(source_counts.live == (source_owns ? 1 : 0));
        CHECK(source_counts.destroyed == 0);
        // 必须释放目标原来的对象；直接覆盖指针会遗失这份资源。
        CHECK(destination_counts.constructed == (destination_owns ? 1 : 0));
        CHECK(destination_counts.live == 0);
        CHECK(destination_counts.destroyed == (destination_owns ? 1 : 0));
        if (source_owns) { CHECK(destination->value == 7); }
    }
    CHECK(source_counts.live == 0);
    CHECK(source_counts.destroyed == (source_owns ? 1 : 0));
    CHECK(destination_counts.live == 0);
    CHECK(destination_counts.destroyed == (destination_owns ? 1 : 0));
}

TEST_CASE("UniquePtr self move assignment preserves ownership without destruction",
          "[unique_ptr][move_assignment][lifetime]")
{
    bool owns = false;
    SECTION("empty owner") {}
    SECTION("owning object") { owns = true; }

    LifetimeCounts counts;
    {
        ObjectOwner owner(owns ? new TrackedObject(counts, 7) : nullptr);
        auto* object = owner.get();
        auto& alias = owner;
        auto& result = (owner = std::move(alias));
        // 本项目明确规定自移动不做资源工作：先删除自己再取指针会悬空。
        CHECK(std::addressof(result) == std::addressof(owner));
        REQUIRE(owner.get() == object);
        CHECK(static_cast<bool>(owner) == owns);
        CHECK(counts.constructed == (owns ? 1 : 0));
        REQUIRE(counts.live == (owns ? 1 : 0));
        CHECK(counts.destroyed == 0);
        if (owns) { CHECK(owner->value == 7); }
    }
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == (owns ? 1 : 0));
}

TEST_CASE("UniquePtr repeated move assignment reuses moved-from owners",
          "[unique_ptr][move_assignment][lifetime]")
{
    LifetimeCounts counts;
    {
        ObjectOwner first(new TrackedObject(counts, 7));
        ObjectOwner second(new TrackedObject(counts, 11));
        ObjectOwner third(new TrackedObject(counts, 13));
        auto* object = first.get();

        second = std::move(first);
        CHECK(first.get() == nullptr);
        CHECK(second.get() == object);
        CHECK(counts.live == 2);
        CHECK(counts.destroyed == 1);

        third = std::move(second);
        CHECK(second.get() == nullptr);
        CHECK(third.get() == object);
        CHECK(counts.live == 1);
        CHECK(counts.destroyed == 2);

        // 已移出的 first 仍然有效，可以再次接管；整个过程没有搬动 T。
        first = std::move(third);
        CHECK(third.get() == nullptr);
        REQUIRE(first.get() == object);
        REQUIRE(counts.live == 1);
        CHECK(first->value == 7);
        CHECK(counts.constructed == 3);
        CHECK(counts.destroyed == 2);
    }
    CHECK(counts.constructed == 3);
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 3);
}

TEST_CASE("UniquePtr move-assigned destination outlives its source",
          "[unique_ptr][move_assignment][lifetime]")
{
    LifetimeCounts counts;
    {
        ObjectOwner destination(new TrackedObject(counts, 11));
        {
            ObjectOwner source(new TrackedObject(counts, 7));
            destination = std::move(source);
            CHECK(source.get() == nullptr);
        }
        REQUIRE(destination.get() != nullptr);
        REQUIRE(counts.live == 1);
        CHECK(destination->value == 7);
        CHECK(counts.constructed == 2);
        CHECK(counts.destroyed == 1);
    }
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 2);
}

TEST_CASE("UniquePtr transferred ownership is released once during unwinding",
          "[unique_ptr][move][lifetime][exception]")
{
    LifetimeCounts counts;
    {
        ObjectOwner source(new TrackedObject(counts, 7));
        const auto leave_with_exception = [&source] {
            ObjectOwner destination(std::move(source));
            throw std::runtime_error("leave moved owner scope");
        };
        REQUIRE_THROWS_AS(leave_with_exception(), std::runtime_error);
        CHECK(source.get() == nullptr);
        CHECK_FALSE(static_cast<bool>(source));
        CHECK(counts.constructed == 1);
        CHECK(counts.live == 0);
        CHECK(counts.destroyed == 1);
    }
    // 空源随后析构不能再次删除已经由目标释放的对象。
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr release from empty is harmless and repeatable",
          "[unique_ptr][release]")
{
    ObjectOwner owner;
    CHECK(owner.release() == nullptr);
    CHECK(owner.get() == nullptr);
    CHECK_FALSE(static_cast<bool>(owner));
    CHECK(owner.release() == nullptr);
}

TEST_CASE("UniquePtr release transfers deletion responsibility without destroying the object",
          "[unique_ptr][release][lifetime]")
{
    LifetimeCounts counts;
    // 标准库所有者仅用于测试清理，确保断言失败时也能回收已交出的资源。
    std::unique_ptr<TrackedObject> released;
    {
        ObjectOwner owner(new TrackedObject(counts, 7));
        auto* object = owner.get();
        released.reset(owner.release());

        REQUIRE(released.get() == object);
        CHECK(owner.get() == nullptr);
        CHECK_FALSE(static_cast<bool>(owner));
        CHECK(owner.release() == nullptr);
        REQUIRE(counts.live == 1);
        CHECK(released->value == 7);
        CHECK(counts.constructed == 1);
        CHECK(counts.destroyed == 0);
    }
    // 原所有者已经析构，被交出的 T 仍存活；最终由接收者负责删除。
    REQUIRE(counts.live == 1);
    CHECK(counts.destroyed == 0);
    CHECK(released->value == 7);
    released.reset();
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr reset clears ownership and repeated empty reset is harmless",
          "[unique_ptr][reset][lifetime]")
{
    bool owns = false;
    SECTION("initially empty") {}
    SECTION("initially owning") { owns = true; }

    LifetimeCounts counts;
    {
        ObjectOwner owner(owns ? new TrackedObject(counts, 7) : nullptr);
        owner.reset();
        CHECK(owner.get() == nullptr);
        CHECK_FALSE(static_cast<bool>(owner));
        CHECK(counts.constructed == (owns ? 1 : 0));
        CHECK(counts.live == 0);
        CHECK(counts.destroyed == (owns ? 1 : 0));

        owner.reset(nullptr);
        owner.reset();
        CHECK(owner.get() == nullptr);
        CHECK(counts.destroyed == (owns ? 1 : 0));
    }
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == (owns ? 1 : 0));
}

TEST_CASE("UniquePtr reset adopts a new object and releases only the old object",
          "[unique_ptr][reset][lifetime]")
{
    bool had_old_object = false;
    SECTION("adopt into empty owner") {}
    SECTION("replace existing object") { had_old_object = true; }

    LifetimeCounts old_counts;
    LifetimeCounts new_counts;
    {
        ObjectOwner owner(had_old_object ? new TrackedObject(old_counts, 7) : nullptr);
        auto* replacement = new TrackedObject(new_counts, 11);
        owner.reset(replacement);

        REQUIRE(owner.get() == replacement);
        CHECK(static_cast<bool>(owner));
        CHECK(old_counts.constructed == (had_old_object ? 1 : 0));
        CHECK(old_counts.live == 0);
        CHECK(old_counts.destroyed == (had_old_object ? 1 : 0));
        CHECK(new_counts.constructed == 1);
        REQUIRE(new_counts.live == 1);
        CHECK(new_counts.destroyed == 0);
        CHECK(owner->value == 11);
        // TrackedObject 不可赋值/移动；reset 只能换所有权，不能改写旧 T 的值。
    }
    CHECK(old_counts.live == 0);
    CHECK(old_counts.destroyed == (had_old_object ? 1 : 0));
    CHECK(new_counts.live == 0);
    CHECK(new_counts.destroyed == 1);
}

TEST_CASE("UniquePtr reset publishes the new pointer before destroying the old object",
          "[unique_ptr][reset][lifetime]")
{
    bool replace_with_object = false;
    SECTION("clear to null") {}
    SECTION("replace with another object") { replace_with_object = true; }

    ResetObservation observation;
    mycppcontainer::UniquePtr<ResetObserver> owner(new ResetObserver);
    auto* replacement = replace_with_object ? new ResetObserver : nullptr;
    owner->owner = std::addressof(owner);
    owner->expected = replacement;
    owner->observation = std::addressof(observation);

    owner.reset(replacement);
    CHECK(observation.destructor_called);
    CHECK(observation.saw_expected_pointer);
    CHECK(owner.get() == replacement);
    // replacement 没有观察回调；正常作用域析构只清理该新对象。
}

TEST_CASE("UniquePtr reset can readopt its released pointer without destruction",
          "[unique_ptr][release][reset][lifetime]")
{
    LifetimeCounts counts;
    {
        ObjectOwner owner(new TrackedObject(counts, 7));
        auto* object = owner.get();
        // 实参求值时 release 已清空所有者，reset 接管的是无人管理的指针。
        // 这不同于非空 owner.reset(owner.get())；后者违反本轮使用前提，不测试。
        owner.reset(owner.release());
        REQUIRE(owner.get() == object);
        REQUIRE(counts.live == 1);
        CHECK(owner->value == 7);
        CHECK(counts.constructed == 1);
        CHECK(counts.destroyed == 0);
    }
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr member swap exchanges ownership without changing object lifetimes",
          "[unique_ptr][swap][lifetime]")
{
    bool first_owns = false;
    bool second_owns = false;
    SECTION("both empty") {}
    SECTION("only first owns") { first_owns = true; }
    SECTION("only second owns") { second_owns = true; }
    SECTION("both own")
    {
        first_owns = true;
        second_owns = true;
    }

    LifetimeCounts first_counts;
    LifetimeCounts second_counts;
    {
        ObjectOwner first(first_owns ? new TrackedObject(first_counts, 7) : nullptr);
        ObjectOwner second(second_owns ? new TrackedObject(second_counts, 11) : nullptr);
        auto* first_object = first.get();
        auto* second_object = second.get();

        first.swap(second);
        REQUIRE(first.get() == second_object);
        REQUIRE(second.get() == first_object);
        CHECK(static_cast<bool>(first) == second_owns);
        CHECK(static_cast<bool>(second) == first_owns);
        REQUIRE(first_counts.live == (first_owns ? 1 : 0));
        REQUIRE(second_counts.live == (second_owns ? 1 : 0));
        CHECK(first_counts.destroyed == 0);
        CHECK(second_counts.destroyed == 0);
        if (second_owns) { CHECK(first->value == 11); }
        if (first_owns) { CHECK(second->value == 7); }

        // 交换回来仍不应构造或销毁 T，也不应该使对象地址改变。
        first.swap(second);
        CHECK(first.get() == first_object);
        CHECK(second.get() == second_object);
        CHECK(first_counts.constructed == (first_owns ? 1 : 0));
        CHECK(second_counts.constructed == (second_owns ? 1 : 0));
        CHECK(first_counts.destroyed == 0);
        CHECK(second_counts.destroyed == 0);
    }
    CHECK(first_counts.live == 0);
    CHECK(second_counts.live == 0);
    CHECK(first_counts.destroyed == (first_owns ? 1 : 0));
    CHECK(second_counts.destroyed == (second_owns ? 1 : 0));
}

TEST_CASE("UniquePtr self swap preserves empty and owning states",
          "[unique_ptr][swap][lifetime]")
{
    bool owns = false;
    SECTION("empty owner") {}
    SECTION("owning object") { owns = true; }

    LifetimeCounts counts;
    {
        ObjectOwner owner(owns ? new TrackedObject(counts, 7) : nullptr);
        auto* object = owner.get();
        owner.swap(owner);
        REQUIRE(owner.get() == object);
        CHECK(static_cast<bool>(owner) == owns);
        CHECK(counts.constructed == (owns ? 1 : 0));
        REQUIRE(counts.live == (owns ? 1 : 0));
        CHECK(counts.destroyed == 0);
        if (owns) { CHECK(owner->value == 7); }
    }
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == (owns ? 1 : 0));
}

TEST_CASE("UniquePtr supports a default-constructed stateless deleter",
          "[unique_ptr][deleter][lifetime]")
{
    LifetimeCounts counts;
    {
        PlainOwner empty;
        CHECK(empty.get() == nullptr);
        PlainOwner owner(new TrackedObject(counts, 7));
        REQUIRE(counts.live == 1);
        CHECK(owner->value == 7);
        CHECK(counts.constructed == 1);
        CHECK(counts.destroyed == 0);
        // 这里不要求 sizeof(owner) == sizeof(T*)；布局优化尚未开始。
    }
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr stores a value deleter supplied by copy or move",
          "[unique_ptr][deleter][lifetime]")
{
    bool move_argument = false;
    SECTION("copy lvalue deleter") {}
    SECTION("move rvalue deleter") { move_argument = true; }

    LifetimeCounts counts;
    DeletionCounts deletions;
    CountingDeleter policy(deletions);
    {
        auto* object = new TrackedObject(counts, 7);
        auto owner = move_argument ? CustomOwner(object, std::move(policy))
                                   : CustomOwner(object, policy);
        CHECK(owner.get() == object);
        CHECK(owner.get_deleter().counts == &deletions);
        CHECK(std::addressof(owner.get_deleter()) != std::addressof(policy));
        CHECK(policy.counts == (move_argument ? nullptr : &deletions));
        CHECK(counts.constructed == 1);
        CHECK(counts.destroyed == 0);
        CHECK(deletions.calls == 0);
    }
    CHECK(deletions.calls == 1);
    CHECK(deletions.null_calls == 0);
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr never invokes a custom deleter for null ownership",
          "[unique_ptr][deleter][boundary]")
{
    DeletionCounts deletions;
    {
        CustomOwner default_empty;
        default_empty.get_deleter().counts = &deletions;
        CustomOwner explicit_empty(nullptr, CountingDeleter(deletions));
        CHECK_FALSE(static_cast<bool>(explicit_empty));
        default_empty.reset();
        explicit_empty.reset(nullptr);
        explicit_empty.reset();
        CHECK(default_empty.release() == nullptr);
        CHECK(explicit_empty.release() == nullptr);
        CHECK(deletions.calls == 0);
    }
    // delete nullptr 无害，并不意味着用户的 deleter(nullptr) 也没有副作用。
    CHECK(deletions.calls == 0);
    CHECK(deletions.null_calls == 0);
}

TEST_CASE("UniquePtr get_deleter exposes the stored policy by reference",
          "[unique_ptr][deleter][access]")
{
    LifetimeCounts counts;
    DeletionCounts original;
    DeletionCounts replacement;
    {
        CustomOwner owner(new TrackedObject(counts, 7), CountingDeleter(original));
        const auto& view = owner;
        CHECK(std::addressof(view.get_deleter()) == std::addressof(owner.get_deleter()));
        CHECK(view.get_deleter().counts == &original);
        // 两个策略都能正确 delete 同一种 T；这里只改变记账位置，不破坏释放配对。
        owner.get_deleter().counts = &replacement;
        CHECK(view.get_deleter().counts == &replacement);
        owner.reset();
        CHECK(original.calls == 0);
        CHECK(replacement.calls == 1);
        CHECK(counts.live == 0);
    }
    CHECK(replacement.calls == 1);
    CHECK(replacement.null_calls == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr reset uses and retains the current custom deleter",
          "[unique_ptr][deleter][reset][lifetime]")
{
    bool initially_owns = false;
    SECTION("initially empty") {}
    SECTION("initially owning") { initially_owns = true; }

    LifetimeCounts old_counts;
    LifetimeCounts new_counts;
    DeletionCounts deletions;
    {
        CustomOwner owner(initially_owns ? new TrackedObject(old_counts, 7) : nullptr,
                          CountingDeleter(deletions));
        auto* replacement = new TrackedObject(new_counts, 11);
        owner.reset(replacement);
        CHECK(owner.get() == replacement);
        CHECK(owner.get_deleter().counts == &deletions);
        CHECK(deletions.calls == (initially_owns ? 1 : 0));
        CHECK(old_counts.live == 0);
        CHECK(old_counts.destroyed == (initially_owns ? 1 : 0));
        REQUIRE(new_counts.live == 1);
        CHECK(new_counts.destroyed == 0);
        CHECK(owner->value == 11);

        owner.reset();
        CHECK(owner.get() == nullptr);
        CHECK(owner.get_deleter().counts == &deletions);
        CHECK(deletions.calls == (initially_owns ? 2 : 1));
        CHECK(new_counts.live == 0);
        CHECK(new_counts.destroyed == 1);
        owner.reset();
    }
    CHECK(deletions.calls == (initially_owns ? 2 : 1));
    CHECK(deletions.null_calls == 0);
}

TEST_CASE("UniquePtr release preserves the custom deleter without invoking it",
          "[unique_ptr][deleter][release][lifetime]")
{
    LifetimeCounts counts;
    DeletionCounts deletions;
    // release 只交出指针；接收方必须另行具备匹配的删除策略。
    std::unique_ptr<TrackedObject, CountingDeleter> receiver(nullptr,
                                                            CountingDeleter(deletions));
    {
        CustomOwner owner(new TrackedObject(counts, 7), CountingDeleter(deletions));
        auto* object = owner.get();
        receiver.reset(owner.release());
        CHECK(receiver.get() == object);
        CHECK(owner.get() == nullptr);
        CHECK(owner.get_deleter().counts == &deletions);
        CHECK(deletions.calls == 0);
        REQUIRE(counts.live == 1);
    }
    CHECK(deletions.calls == 0);
    receiver.reset();
    CHECK(deletions.calls == 1);
    CHECK(deletions.null_calls == 0);
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}

TEST_CASE("UniquePtr move construction transfers pointer and deleter together",
          "[unique_ptr][deleter][move][lifetime]")
{
    bool owns = false;
    SECTION("empty source still carries a policy") {}
    SECTION("owning source") { owns = true; }

    LifetimeCounts counts;
    DeletionCounts deletions;
    {
        CustomOwner source(owns ? new TrackedObject(counts, 7) : nullptr,
                           CountingDeleter(deletions));
        auto* object = source.get();
        {
            CustomOwner destination(std::move(source));
            CHECK(destination.get() == object);
            CHECK(destination.get_deleter().counts == &deletions);
            CHECK(source.get() == nullptr);
            // 这是该测试删除器的移动后状态，不是对所有 Deleter 的统一要求。
            CHECK(source.get_deleter().counts == nullptr);
            CHECK(deletions.calls == 0);
            CHECK(counts.live == (owns ? 1 : 0));
            CHECK(counts.destroyed == 0);
        }
        CHECK(deletions.calls == (owns ? 1 : 0));
        CHECK(counts.live == 0);
    }
    CHECK(deletions.calls == (owns ? 1 : 0));
    CHECK(deletions.null_calls == 0);
    CHECK(counts.destroyed == (owns ? 1 : 0));
}

TEST_CASE("UniquePtr move assignment cleans up with the old deleter before replacing it",
          "[unique_ptr][deleter][move_assignment][lifetime]")
{
    bool source_owns = false;
    bool destination_owns = false;
    SECTION("both empty") {}
    SECTION("owning source and empty destination") { source_owns = true; }
    SECTION("empty source and owning destination") { destination_owns = true; }
    SECTION("both owning") { source_owns = true; destination_owns = true; }

    LifetimeCounts source_counts;
    LifetimeCounts destination_counts;
    DeletionCounts source_deletions;
    DeletionCounts destination_deletions;
    {
        CustomOwner source(source_owns ? new TrackedObject(source_counts, 7) : nullptr,
                           CountingDeleter(source_deletions));
        CustomOwner destination(destination_owns ? new TrackedObject(destination_counts, 11)
                                                : nullptr,
                                CountingDeleter(destination_deletions));
        auto* object = source.get();
        auto& result = (destination = std::move(source));
        CHECK(std::addressof(result) == std::addressof(destination));
        CHECK(destination.get() == object);
        CHECK(source.get() == nullptr);
        CHECK(destination.get_deleter().counts == &source_deletions);
        CHECK(source.get_deleter().counts == nullptr);
        // 若先覆盖目标的 deleter，旧资源就会被错误地记到源的删除策略上。
        CHECK(destination_deletions.calls == (destination_owns ? 1 : 0));
        CHECK(source_deletions.calls == 0);
        CHECK(destination_counts.live == 0);
        CHECK(destination_counts.destroyed == (destination_owns ? 1 : 0));
        CHECK(source_counts.live == (source_owns ? 1 : 0));
        CHECK(source_counts.destroyed == 0);
        destination.reset();
        CHECK(source_deletions.calls == (source_owns ? 1 : 0));
    }
    CHECK(source_counts.live == 0);
    CHECK(source_counts.destroyed == (source_owns ? 1 : 0));
    CHECK(destination_deletions.calls == (destination_owns ? 1 : 0));
    CHECK(source_deletions.calls == (source_owns ? 1 : 0));
    CHECK(source_deletions.null_calls == 0);
    CHECK(destination_deletions.null_calls == 0);
}

TEST_CASE("UniquePtr swap exchanges custom deleters even when a pointer is null",
          "[unique_ptr][deleter][swap][lifetime]")
{
    bool first_owns = false;
    bool second_owns = false;
    SECTION("both empty") {}
    SECTION("only first owns") { first_owns = true; }
    SECTION("only second owns") { second_owns = true; }
    SECTION("both own") { first_owns = true; second_owns = true; }

    LifetimeCounts first_counts;
    LifetimeCounts second_counts;
    DeletionCounts first_deletions;
    DeletionCounts second_deletions;
    {
        CustomOwner first(first_owns ? new TrackedObject(first_counts, 7) : nullptr,
                          CountingDeleter(first_deletions));
        CustomOwner second(second_owns ? new TrackedObject(second_counts, 11) : nullptr,
                           CountingDeleter(second_deletions));
        auto* first_object = first.get();
        auto* second_object = second.get();
        first.swap(second);
        CHECK(first.get() == second_object);
        CHECK(second.get() == first_object);
        CHECK(first.get_deleter().counts == &second_deletions);
        CHECK(second.get_deleter().counts == &first_deletions);
        CHECK(first_deletions.calls == 0);
        CHECK(second_deletions.calls == 0);
        CHECK(first_counts.live == (first_owns ? 1 : 0));
        CHECK(second_counts.live == (second_owns ? 1 : 0));
        CHECK(first_counts.destroyed == 0);
        CHECK(second_counts.destroyed == 0);
    }
    CHECK(first_deletions.calls == (first_owns ? 1 : 0));
    CHECK(second_deletions.calls == (second_owns ? 1 : 0));
    CHECK(first_deletions.null_calls == 0);
    CHECK(second_deletions.null_calls == 0);
    CHECK(first_counts.live == 0);
    CHECK(second_counts.live == 0);
    CHECK(first_counts.destroyed == (first_owns ? 1 : 0));
    CHECK(second_counts.destroyed == (second_owns ? 1 : 0));
}

TEST_CASE("UniquePtr self move and self swap preserve the custom deleter",
          "[unique_ptr][deleter][boundary][lifetime]")
{
    bool owns = false;
    SECTION("empty owner") {}
    SECTION("owning object") { owns = true; }

    LifetimeCounts counts;
    DeletionCounts deletions;
    {
        CustomOwner owner(owns ? new TrackedObject(counts, 7) : nullptr,
                          CountingDeleter(deletions));
        auto* object = owner.get();
        auto& alias = owner;
        owner = std::move(alias);
        CHECK(owner.get() == object);
        CHECK(owner.get_deleter().counts == &deletions);
        owner.swap(alias);
        CHECK(owner.get() == object);
        CHECK(owner.get_deleter().counts == &deletions);
        CHECK(deletions.calls == 0);
        CHECK(counts.live == (owns ? 1 : 0));
        CHECK(counts.destroyed == 0);
    }
    CHECK(deletions.calls == (owns ? 1 : 0));
    CHECK(deletions.null_calls == 0);
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == (owns ? 1 : 0));
}

TEST_CASE("UniquePtr invokes its custom deleter once during stack unwinding",
          "[unique_ptr][deleter][exception][lifetime]")
{
    LifetimeCounts counts;
    DeletionCounts deletions;
    const auto leave_with_exception = [&] {
        CustomOwner owner(new TrackedObject(counts, 7), CountingDeleter(deletions));
        throw std::runtime_error("leave custom owner scope");
    };
    REQUIRE_THROWS_AS(leave_with_exception(), std::runtime_error);
    CHECK(deletions.calls == 1);
    CHECK(deletions.null_calls == 0);
    CHECK(counts.constructed == 1);
    CHECK(counts.live == 0);
    CHECK(counts.destroyed == 1);
}
