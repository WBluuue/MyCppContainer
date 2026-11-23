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

// 无默认构造、不可赋值：push_back 必须在原始存储中拷贝构造元素。
// 记录成功构造的对象；失败的构造不能计入 live，也不会调用该对象的析构函数。
struct CopyProbe {
    inline static int live = 0;
    inline static int copies = 0;
    inline static int destroyed = 0;
    inline static int last_copied_value = 0;
    inline static bool throw_on_copy = false;

    int value;

    CopyProbe() = delete;
    explicit CopyProbe(int v) : value(v) { ++live; }

    CopyProbe(const CopyProbe& other) : value(other.value)
    {
        if (throw_on_copy)
        {
            throw std::runtime_error("CopyProbe copy failed");
        }
        ++live;
        ++copies;
        last_copied_value = value;
    }

    CopyProbe& operator=(const CopyProbe&) = delete;

    ~CopyProbe()
    {
        --live;
        ++destroyed;
    }

    // 每个测试开始时调用，此时不应有上一个测试的 CopyProbe 对象存活。
    static void reset() noexcept
    {
        live = 0;
        copies = 0;
        destroyed = 0;
        last_copied_value = 0;
        throw_on_copy = false;
    }
};

} // namespace

static_assert(!std::is_default_constructible_v<NoDefault>);
static_assert(!std::is_default_constructible_v<CopyProbe>);
static_assert(!std::is_copy_assignable_v<CopyProbe>);

// 所有权接口保持无异常移动构造和移动赋值；复制操作仍然禁止。
static_assert(!std::is_copy_constructible_v<vector<int>>);
static_assert(!std::is_copy_assignable_v<vector<int>>);
static_assert(std::is_nothrow_move_constructible_v<vector<int>>);
static_assert(std::is_nothrow_move_assignable_v<vector<int>>);

// 下标访问返回元素引用；const 容器只提供 const 引用。
// decltype 和 noexcept 中的表达式不会实际执行，不会访问空容器。
static_assert(std::is_same_v<decltype(std::declval<vector<int>&>()[0]), int&>);
static_assert(std::is_same_v<decltype(std::declval<const vector<int>&>()[0]),
                             const int&>);
static_assert(noexcept(std::declval<vector<int>&>()[0]));
static_assert(noexcept(std::declval<const vector<int>&>()[0]));

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

TEST_CASE("Vector push_back uses reserved capacity", "[vector][push_back]")
{
    vector<int> v;
    v.reserve(3);

    v.push_back(7);
    CHECK(v.size() == 1);
    CHECK(v.capacity() == 3);
    CHECK_FALSE(v.empty());

    // 最后一个可用位置仍允许构造；本轮追加不会改变容量。
    v.push_back(11);
    v.push_back(13);
    CHECK(v.size() == 3);
    CHECK(v.capacity() == 3);
    CHECK_FALSE(v.empty());
}

TEST_CASE("Vector push_back rejects insufficient capacity before construction",
          "[vector][push_back]")
{
    CopyProbe::reset();
    {
        CopyProbe value(7);
        {
            vector<CopyProbe> v;

            SECTION("no storage has been reserved")
            {
                CHECK(v.capacity() == 0);
            }
            SECTION("reserved storage is full")
            {
                v.reserve(1);
                v.push_back(value);
            }

            const auto old_size = v.size();
            const auto old_capacity = v.capacity();
            const auto old_copies = CopyProbe::copies;
            const auto old_live = CopyProbe::live;

            // 容量不足必须先被拒绝，不能尝试拷贝，也不能自动扩容。
            CopyProbe::throw_on_copy = true;
            REQUIRE_THROWS_AS(v.push_back(value), std::length_error);
            CopyProbe::throw_on_copy = false;

            CHECK(v.size() == old_size);
            CHECK(v.capacity() == old_capacity);
            CHECK(v.empty() == (old_size == 0));
            CHECK(CopyProbe::copies == old_copies);
            CHECK(CopyProbe::live == old_live);
            CHECK(CopyProbe::destroyed == 0);
        }
        CHECK(CopyProbe::live == 1);
    }
    CHECK(CopyProbe::live == 0);
}

TEST_CASE("Vector push_back constructs and destroys only live elements",
          "[vector][push_back][lifetime]")
{
    CopyProbe::reset();
    {
        CopyProbe first(7);
        CopyProbe second(11);
        {
            vector<CopyProbe> v;
            v.reserve(4);
            CHECK(CopyProbe::live == 2);
            CHECK(CopyProbe::copies == 0);

            v.push_back(first);
            CHECK(v.size() == 1);
            CHECK(CopyProbe::copies == 1);
            CHECK(CopyProbe::last_copied_value == 7);

            v.push_back(second);
            CHECK(v.size() == 2);
            CHECK(v.capacity() == 4);
            CHECK(CopyProbe::copies == 2);
            CHECK(CopyProbe::last_copied_value == 11);
            CHECK(CopyProbe::live == 4);
            CHECK(CopyProbe::destroyed == 0);
        }

        // 容量为 4，但只构造了 2 个元素；析构只能处理这 2 个。
        CHECK(CopyProbe::live == 2);
        CHECK(CopyProbe::destroyed == 2);
    }
    CHECK(CopyProbe::live == 0);
    CHECK(CopyProbe::destroyed == 4);
}

TEST_CASE("Vector empty reserve supports move-only element types",
          "[vector][reserve]")
{
    // 没有元素可搬迁，预留原始存储不应要求 T 可拷贝。
    // 即使某个循环运行零次，循环体内的模板调用通常仍需要通过编译。
    vector<std::unique_ptr<int>> v;
    REQUIRE_NOTHROW(v.reserve(4));
    REQUIRE_NOTHROW(v.reserve(9));

    CHECK(v.size() == 0);
    CHECK(v.capacity() == 9);
    CHECK(v.empty());
}

TEST_CASE("Vector subscript reads and modifies stored values",
          "[vector][access]")
{
    vector<int> v;
    v.reserve(4);
    v.push_back(7);
    v.push_back(11);
    v.push_back(13);
    REQUIRE(v.size() == 3);

    // 合法下标由 size 决定；虽然容量为 4，但这里不能访问 v[3]。
    CHECK(v[0] == 7);
    CHECK(v[1] == 11);
    CHECK(v[2] == 13);

    v[1] = 42;
    const auto& cv = v;
    CHECK(cv[0] == 7);
    CHECK(cv[1] == 42);
    CHECK(cv[2] == 13);
    CHECK(cv.size() == 3);
    CHECK(cv.capacity() == 4);
}

TEST_CASE("Vector subscript refers to elements without copying",
          "[vector][access]")
{
    CopyProbe::reset();
    {
        CopyProbe value(7);
        {
            vector<CopyProbe> v;
            v.reserve(2);
            v.push_back(value);
            REQUIRE(v.size() == 1);

            const auto& cv = v;
            auto& writable = v[0];
            const auto& readable = cv[0];

            // 两个重载都访问同一个对象，不创建元素副本。
            CHECK(std::addressof(writable) == std::addressof(readable));
            writable.value = 11;
            CHECK(readable.value == 11);
            CHECK(value.value == 7);
            CHECK(CopyProbe::copies == 1);
            CHECK(CopyProbe::live == 2);
            CHECK(CopyProbe::destroyed == 0);
        }
        CHECK(CopyProbe::live == 1);
        CHECK(CopyProbe::destroyed == 1);
    }
    CHECK(CopyProbe::live == 0);
    CHECK(CopyProbe::destroyed == 2);
}

TEST_CASE("Vector references remain valid within reserved capacity",
          "[vector][access]")
{
    vector<int> v;
    v.reserve(3);
    v.push_back(7);
    REQUIRE(v.size() == 1);
    int* first = std::addressof(v[0]);

    REQUIRE_NOTHROW(v.reserve(0));
    REQUIRE_NOTHROW(v.reserve(1));
    REQUIRE_NOTHROW(v.reserve(3));
    CHECK(std::addressof(v[0]) == first);

    // 参数引用容器自身的元素；容量足够时，追加不应使这个引用失效。
    v.push_back(v[0]);
    v.push_back(11);
    REQUIRE(v.size() == 3);
    CHECK(v.capacity() == 3);
    CHECK(std::addressof(v[0]) == first);
    CHECK(v[0] == 7);
    CHECK(v[1] == 7);
    CHECK(v[2] == 11);
}

TEST_CASE("Vector moves preserve element values and addresses",
          "[vector][access][move]")
{
    vector<int> source;
    source.reserve(4);
    source.push_back(7);
    source.push_back(11);
    REQUIRE(source.size() == 2);
    int* first = std::addressof(source[0]);

    vector<int> moved(std::move(source));
    REQUIRE(moved.size() == 2);
    CHECK(moved[0] == 7);
    CHECK(moved[1] == 11);
    CHECK(std::addressof(moved[0]) == first);
    CHECK(source.size() == 0);
    CHECK(source.capacity() == 0);

    vector<int> destination;
    destination.reserve(2);
    destination.push_back(99);
    destination = std::move(moved);

    // 固定使用 std::allocator<T> 时，移动直接接管原分配块。
    REQUIRE(destination.size() == 2);
    CHECK(destination.capacity() == 4);
    CHECK(destination[0] == 7);
    CHECK(destination[1] == 11);
    CHECK(std::addressof(destination[0]) == first);
    CHECK(moved.size() == 0);
    CHECK(moved.capacity() == 0);
}

TEST_CASE("Vector failed append preserves existing values and addresses",
          "[vector][access]")
{
    CopyProbe::reset();
    {
        CopyProbe value(7);
        {
            vector<CopyProbe> v;
            v.reserve(3);
            v.push_back(value);
            REQUIRE(v.size() == 1);
            const CopyProbe* first = std::addressof(v[0]);

            value.value = 11;
            CopyProbe::throw_on_copy = true;
            REQUIRE_THROWS_AS(v.push_back(value), std::runtime_error);
            CopyProbe::throw_on_copy = false;

            // 除了计数正确，原元素的实际值和地址也必须保持。
            REQUIRE(v.size() == 1);
            CHECK(v.capacity() == 3);
            CHECK(v[0].value == 7);
            CHECK(std::addressof(v[0]) == first);
            CHECK(CopyProbe::live == 2);
            CHECK(CopyProbe::copies == 1);

            v.push_back(value);
            REQUIRE(v.size() == 2);
            CHECK(v[0].value == 7);
            CHECK(v[1].value == 11);
            CHECK(std::addressof(v[0]) == first);
        }
        CHECK(CopyProbe::live == 1);
    }
    CHECK(CopyProbe::live == 0);
}

TEST_CASE("Vector failed push_back preserves live elements",
          "[vector][push_back][lifetime]")
{
    CopyProbe::reset();
    {
        CopyProbe value(7);
        {
            vector<CopyProbe> v;
            v.reserve(3);
            v.push_back(value);

            value.value = 11;
            CopyProbe::throw_on_copy = true;
            REQUIRE_THROWS_AS(v.push_back(value), std::runtime_error);
            CopyProbe::throw_on_copy = false;

            // 第二个元素构造失败，不能增加 size，也不能析构失败的槽位。
            CHECK(v.size() == 1);
            CHECK(v.capacity() == 3);
            CHECK(CopyProbe::live == 2);
            CHECK(CopyProbe::copies == 1);
            CHECK(CopyProbe::destroyed == 0);
            CHECK(CopyProbe::last_copied_value == 7);

            // 失败后，同一个未使用的位置仍然可以成功构造元素。
            REQUIRE_NOTHROW(v.push_back(value));
            CHECK(v.size() == 2);
            CHECK(v.capacity() == 3);
            CHECK(CopyProbe::live == 3);
            CHECK(CopyProbe::copies == 2);
            CHECK(CopyProbe::last_copied_value == 11);
        }
        CHECK(CopyProbe::live == 1);
        CHECK(CopyProbe::destroyed == 2);
    }
    CHECK(CopyProbe::live == 0);
    CHECK(CopyProbe::destroyed == 3);
}

TEST_CASE("Vector reserve rejects growth while elements are alive",
          "[vector][push_back][lifetime]")
{
    CopyProbe::reset();
    {
        CopyProbe value(7);
        {
            vector<CopyProbe> v;
            v.reserve(3);
            v.push_back(value);

            // 有元素时，不超过现有容量的 reserve 仍然是合法的无操作。
            REQUIRE_NOTHROW(v.reserve(0));
            REQUIRE_NOTHROW(v.reserve(1));
            REQUIRE_NOTHROW(v.reserve(3));

            // 本轮尚未实现元素搬迁，非空扩容必须被拒绝，不能丢弃旧对象。
            REQUIRE_THROWS_AS(v.reserve(4), std::logic_error);
            CHECK(v.size() == 1);
            CHECK(v.capacity() == 3);
            CHECK(CopyProbe::live == 2);
            CHECK(CopyProbe::copies == 1);
            CHECK(CopyProbe::destroyed == 0);

            REQUIRE_NOTHROW(v.push_back(value));
            CHECK(v.size() == 2);
            CHECK(v.capacity() == 3);
            CHECK(CopyProbe::live == 3);
        }
        CHECK(CopyProbe::live == 1);
    }
    CHECK(CopyProbe::live == 0);
}

TEST_CASE("Vector move construction transfers live elements without copying",
          "[vector][push_back][lifetime]")
{
    CopyProbe::reset();
    {
        CopyProbe value(7);
        {
            vector<CopyProbe> source;
            source.reserve(4);
            source.push_back(value);
            source.push_back(value);
            {
                vector<CopyProbe> destination(std::move(source));

                // 移动的是分配块所有权，块内的元素不应再次构造或提前析构。
                CHECK(destination.size() == 2);
                CHECK(destination.capacity() == 4);
                CHECK(source.size() == 0);
                CHECK(source.capacity() == 0);
                CHECK(source.empty());
                CHECK(CopyProbe::copies == 2);
                CHECK(CopyProbe::live == 3);
                CHECK(CopyProbe::destroyed == 0);

                source.reserve(1);
                source.push_back(value);
                CHECK(source.size() == 1);
                CHECK(destination.size() == 2);
                CHECK(CopyProbe::live == 4);
                CHECK(CopyProbe::copies == 3);
            }
            CHECK(CopyProbe::live == 2);
            CHECK(CopyProbe::destroyed == 2);
        }
        CHECK(CopyProbe::live == 1);
        CHECK(CopyProbe::destroyed == 3);
    }
    CHECK(CopyProbe::live == 0);
    CHECK(CopyProbe::destroyed == 4);
}

TEST_CASE("Vector move assignment destroys replaced live elements",
          "[vector][push_back][lifetime]")
{
    CopyProbe::reset();
    {
        CopyProbe value(7);
        {
            vector<CopyProbe> source;
            source.reserve(4);
            source.push_back(value);
            source.push_back(value);

            vector<CopyProbe> destination;
            destination.reserve(3);
            destination.push_back(value);

            auto& assigned = (destination = std::move(source));

            // 目标的旧元素要析构；源的两个元素随存储转移，不能被析构。
            CHECK(&assigned == &destination);
            CHECK(destination.size() == 2);
            CHECK(destination.capacity() == 4);
            CHECK(source.size() == 0);
            CHECK(source.capacity() == 0);
            CHECK(CopyProbe::copies == 3);
            CHECK(CopyProbe::live == 3);
            CHECK(CopyProbe::destroyed == 1);

            auto& alias = destination;
            destination = std::move(alias);

            // 有元素的自移动仍必须保持原状态，不能先销毁自己的元素。
            CHECK(destination.size() == 2);
            CHECK(destination.capacity() == 4);
            CHECK(CopyProbe::live == 3);
            CHECK(CopyProbe::copies == 3);
            CHECK(CopyProbe::destroyed == 1);
        }
        CHECK(CopyProbe::live == 1);
        CHECK(CopyProbe::destroyed == 3);
    }
    CHECK(CopyProbe::live == 0);
    CHECK(CopyProbe::destroyed == 4);
}
