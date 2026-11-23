#include<catch2/catch_test_macros.hpp>
#include<mycppcontainer/vector.hpp>
#include<cstddef>
#include<limits>
#include<memory>
#include<stdexcept>
#include<type_traits>
#include<utility>

template<class T>
using vector=mycppcontainer::Vector<T>;

namespace {

// reserve 只分配存储，不应要求元素具有默认构造函数。
struct NoDefault {
    NoDefault() = delete;
    explicit NoDefault(int) {}
};

// 统计元素的构造和析构；这些计数不代表内存分配、释放次数。
struct Tracked {
    inline static int constructed = 0;
    inline static int destroyed = 0;

    Tracked() { ++constructed; }
    Tracked(const Tracked&) { ++constructed; }
    Tracked(Tracked&&) noexcept { ++constructed; }

    ~Tracked() { ++destroyed; }
};

} // namespace

static_assert(!std::is_default_constructible_v<NoDefault>);

// 本轮只开放无异常移动构造，复制和两种赋值操作仍然禁止。
static_assert(!std::is_copy_constructible_v<vector<int>>);
static_assert(!std::is_copy_assignable_v<vector<int>>);
static_assert(std::is_nothrow_move_constructible_v<vector<int>>);
static_assert(!std::is_move_assignable_v<vector<int>>);

TEST_CASE("Vector default construction","[vector]")
{
    vector<int>v;

    CHECK(v.empty());
    CHECK(v.size()==0);
    CHECK(v.capacity()==0);
}

TEST_CASE("Vector const access","[vector]")
{
    const vector<int>v;

    CHECK(v.empty());
    CHECK(v.size()==0);
    CHECK(v.capacity()==0);
}

TEST_CASE("Vector reserve capacity","[vector]")
{
    vector<int>v;

    v.reserve(0);
    CHECK(v.size()==0);
    CHECK(v.capacity()==0);

    v.reserve(4);
    CHECK(v.size()==0);
    CHECK(v.capacity()==4);

    v.reserve(4);
    CHECK(v.size()==0);
    CHECK(v.capacity()==4);

    v.reserve(2);
    CHECK(v.size()==0);
    CHECK(v.capacity()==4);

    v.reserve(9);
    CHECK(v.size()==0);
    CHECK(v.capacity()==9); 
}

TEST_CASE("Vector reserve zero preserves allocated capacity", "[vector]")
{
    vector<int> v;
    v.reserve(4);
    v.reserve(0);

    CHECK(v.capacity() == 4);
    CHECK(v.size() == 0);
    CHECK(v.empty());
}

TEST_CASE("Vector reserve supports non-default-constructible elements",
          "[vector]")
{
    vector<NoDefault> v;

    REQUIRE_NOTHROW(v.reserve(4));
    REQUIRE_NOTHROW(v.reserve(9));

    CHECK(v.capacity() == 9);
    CHECK(v.size() == 0);
    CHECK(v.empty());
}

TEST_CASE("Vector reserve does not construct or destroy elements",
          "[vector]")
{
    Tracked::constructed = 0;
    Tracked::destroyed = 0;

    {
        vector<Tracked> v;
        v.reserve(4);
        v.reserve(9);

        CHECK(v.capacity() == 9);
        CHECK(v.size() == 0);
        CHECK(v.empty());

        CHECK(Tracked::constructed == 0);
        CHECK(Tracked::destroyed == 0);
    }

    CHECK(Tracked::constructed == 0);
    CHECK(Tracked::destroyed == 0);
}

TEST_CASE("Vector oversized reserve preserves state", "[vector]")
{
    using Alloc = std::allocator<int>;
    using Traits = std::allocator_traits<Alloc>;

    Alloc alloc;
    const auto limit = Traits::max_size(alloc);

    // 先确认 limit + 1 可表示，避免测试输入本身发生整数溢出。
    REQUIRE(limit < std::numeric_limits<std::size_t>::max());

    vector<int> v;
    v.reserve(4);
    const auto old_capacity = v.capacity();

    REQUIRE_THROWS_AS(v.reserve(limit + 1), std::length_error);

    CHECK(v.capacity() == old_capacity);
    CHECK(v.size() == 0);
    CHECK(v.empty());

    REQUIRE_NOTHROW(v.reserve(9));

    CHECK(v.capacity() == 9);
    CHECK(v.size() == 0);
    CHECK(v.empty());
}

TEST_CASE("Vector move construction from default state", "[vector][move]")
{
    vector<int> source;
    vector<int> destination(std::move(source));

    // 没有分配块可转移时，双方都应保持默认空状态。
    CHECK(source.capacity() == 0);
    CHECK(source.size() == 0);
    CHECK(source.empty());
    CHECK(destination.capacity() == 0);
    CHECK(destination.size() == 0);
    CHECK(destination.empty());
}

TEST_CASE("Vector move construction transfers reserved capacity",
          "[vector][move]")
{
    vector<int> source;
    source.reserve(4);

    // std::move 只使表达式能够匹配移动构造，资源转移由构造函数完成。
    vector<int> destination(std::move(source));

    CHECK(destination.capacity() == 4);
    CHECK(destination.size() == 0);
    CHECK(destination.empty());

    // 源对象仍然存活，但已经交出存储，必须恢复本项目约定的空状态。
    CHECK(source.capacity() == 0);
    CHECK(source.size() == 0);
    CHECK(source.empty());
}

TEST_CASE("Vector source and destination remain independent after move",
          "[vector][move]")
{
    vector<int> source;
    source.reserve(4);
    vector<int> destination(std::move(source));

    // 源对象可以重新分配；随后目标扩容也不应改变源对象的状态。
    REQUIRE_NOTHROW(source.reserve(2));
    CHECK(source.capacity() == 2);
    CHECK(destination.capacity() == 4);

    REQUIRE_NOTHROW(destination.reserve(9));
    CHECK(source.capacity() == 2);
    CHECK(source.size() == 0);
    CHECK(source.empty());
    CHECK(destination.capacity() == 9);
    CHECK(destination.size() == 0);
    CHECK(destination.empty());
}

TEST_CASE("Vector moved-from source remains usable after destination destruction",
          "[vector][move]")
{
    vector<int> source;
    source.reserve(4);

    {
        vector<int> destination(std::move(source));
        CHECK(destination.capacity() == 4);
        CHECK(source.capacity() == 0);
    } // 目标先析构，原分配块应由目标释放。

    // 源对象此时仍有效，重新分配不应再次释放已经交出的存储。
    REQUIRE_NOTHROW(source.reserve(2));
    CHECK(source.capacity() == 2);
    CHECK(source.size() == 0);
    CHECK(source.empty());
}

TEST_CASE("Vector destination remains usable after source destruction",
          "[vector][move]")
{
    auto destination = [] {
        vector<int> source;
        source.reserve(4);

        // 返回表达式明确调用移动构造；source 在 lambda 返回时先析构。
        return vector<int>(std::move(source));
    }();

    CHECK(destination.capacity() == 4);
    CHECK(destination.size() == 0);
    CHECK(destination.empty());

    // 源对象析构后，目标仍应独立拥有有效存储，能够继续扩容。
    REQUIRE_NOTHROW(destination.reserve(9));
    CHECK(destination.capacity() == 9);
    CHECK(destination.size() == 0);
    CHECK(destination.empty());
}

TEST_CASE("Vector move construction does not construct or destroy elements",
          "[vector][move]")
{
    Tracked::constructed = 0;
    Tracked::destroyed = 0;

    {
        vector<Tracked> source;
        source.reserve(4);
        vector<Tracked> destination(std::move(source));

        // 本轮容器没有元素：转移原始存储不应调用任何 T 的构造或析构。
        CHECK(source.capacity() == 0);
        CHECK(source.size() == 0);
        CHECK(source.empty());
        CHECK(destination.capacity() == 4);
        CHECK(destination.size() == 0);
        CHECK(destination.empty());
        CHECK(Tracked::constructed == 0);
        CHECK(Tracked::destroyed == 0);
    }

    // 两个容器都析构后仍为零，不能对未构造的容量区域析构元素。
    CHECK(Tracked::constructed == 0);
    CHECK(Tracked::destroyed == 0);
}
