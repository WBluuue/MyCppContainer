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

// 本轮开放无异常移动构造和移动赋值；复制操作仍然禁止。
static_assert(!std::is_copy_constructible_v<vector<int>>);
static_assert(!std::is_copy_assignable_v<vector<int>>);
static_assert(std::is_nothrow_move_constructible_v<vector<int>>);
static_assert(std::is_nothrow_move_assignable_v<vector<int>>);

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

TEST_CASE("Vector move assignment replaces reserved storage",
          "[vector][move_assignment]")
{
    vector<int> source;
    vector<int> destination;

    // SECTION 分别运行四种容量组合；每次都会执行下方的赋值和共享断言。
    // 本轮所有对象的 size 都是 0，具有容量不代表已经构造了元素。
    SECTION("neither object owns storage")
    {
        CHECK(source.capacity() == 0);
        CHECK(destination.capacity() == 0);
    }
    SECTION("only source owns storage")
    {
        source.reserve(4);
    }
    SECTION("only destination owns storage")
    {
        destination.reserve(9);
    }
    SECTION("both objects own storage")
    {
        source.reserve(4);
        destination.reserve(9);
    }

    const auto expected_capacity = source.capacity();
    auto& assigned = (destination = std::move(source));

    // 赋值返回目标自身的左值引用；目标容量来自源对象，而非保留原容量。
    CHECK(&assigned == &destination);
    CHECK(destination.capacity() == expected_capacity);
    CHECK(destination.size() == 0);
    CHECK(destination.empty());

    // 不同对象之间转移所有权后，源对象恢复本项目约定的零容量空状态。
    CHECK(source.capacity() == 0);
    CHECK(source.size() == 0);
    CHECK(source.empty());
}

TEST_CASE("Vector objects remain reusable after repeated move assignment",
          "[vector][move_assignment]")
{
    vector<int> source;
    source.reserve(4);
    vector<int> destination;
    destination.reserve(9);

    destination = std::move(source);

    // 双方可以各自重新预留容量，不能继续共享同一分配块的所有权。
    REQUIRE_NOTHROW(source.reserve(2));
    CHECK(source.capacity() == 2);
    CHECK(destination.capacity() == 4);
    REQUIRE_NOTHROW(destination.reserve(12));
    CHECK(source.capacity() == 2);
    CHECK(source.size() == 0);
    CHECK(source.empty());
    CHECK(destination.capacity() == 12);
    CHECK(destination.size() == 0);
    CHECK(destination.empty());

    // 再次赋值也必须替换目标当前的存储，不能只处理第一次赋值。
    destination = std::move(source);
    CHECK(destination.capacity() == 2);
    CHECK(destination.size() == 0);
    CHECK(destination.empty());
    CHECK(source.capacity() == 0);
    CHECK(source.size() == 0);
    CHECK(source.empty());
}

TEST_CASE("Vector self move assignment preserves state",
          "[vector][move_assignment]")
{
    vector<int> v;

    SECTION("without storage")
    {
        CHECK(v.capacity() == 0);
    }
    SECTION("with reserved storage")
    {
        v.reserve(4);
    }

    const auto previous_capacity = v.capacity();
    // 别名仍指向同一个对象。本项目规定自移动赋值不改变状态。
    auto& alias = v;
    auto& assigned = (v = std::move(alias));

    CHECK(&assigned == &v);
    CHECK(v.capacity() == previous_capacity);
    CHECK(v.size() == 0);
    CHECK(v.empty());

    // 自移动后继续使用，检查是否遗留了错误的所有权状态。
    REQUIRE_NOTHROW(v.reserve(previous_capacity + 3));
    CHECK(v.capacity() == previous_capacity + 3);
    CHECK(v.size() == 0);
    CHECK(v.empty());
}

TEST_CASE("Vector move assignment supports either destruction order",
          "[vector][move_assignment]")
{
    SECTION("destination is destroyed first")
    {
        vector<int> source;
        source.reserve(4);

        {
            vector<int> destination;
            destination.reserve(9);
            destination = std::move(source);
            CHECK(destination.capacity() == 4);
            CHECK(source.capacity() == 0);
        } // 目标释放接管的分配块，源对象仍存活。

        REQUIRE_NOTHROW(source.reserve(2));
        CHECK(source.capacity() == 2);
        CHECK(source.size() == 0);
        CHECK(source.empty());
    }

    SECTION("source is destroyed first")
    {
        vector<int> destination;
        destination.reserve(9);

        {
            vector<int> source;
            source.reserve(4);
            destination = std::move(source);
            CHECK(source.capacity() == 0);
        } // 源对象析构不能释放已经交给目标的存储。

        CHECK(destination.capacity() == 4);
        REQUIRE_NOTHROW(destination.reserve(12));
        CHECK(destination.capacity() == 12);
        CHECK(destination.size() == 0);
        CHECK(destination.empty());
    }
}

TEST_CASE("Vector move assignment does not construct or destroy elements",
          "[vector][move_assignment]")
{
    Tracked::constructed = 0;
    Tracked::destroyed = 0;

    {
        vector<Tracked> source;
        source.reserve(4);
        vector<Tracked> destination;
        destination.reserve(9);

        destination = std::move(source);

        // 目标原有的是原始存储，没有元素可析构；接管源存储也不构造元素。
        CHECK(destination.capacity() == 4);
        CHECK(destination.size() == 0);
        CHECK(destination.empty());
        CHECK(source.capacity() == 0);
        CHECK(source.size() == 0);
        CHECK(source.empty());
        CHECK(Tracked::constructed == 0);
        CHECK(Tracked::destroyed == 0);
    }

    // 这些计数只验证元素生命周期，不能单独证明目标旧存储没有泄漏。
    CHECK(Tracked::constructed == 0);
    CHECK(Tracked::destroyed == 0);
}
