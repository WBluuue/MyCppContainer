#include<catch2/catch_test_macros.hpp>
#include<mycppcontainer/vector.hpp>
#include<cstddef>
#include<limits>
#include<memory>
#include<stdexcept>
#include<type_traits>

template<class T>
using vector=mycppcontainer::Vector<T>;

namespace {

struct NoDefault {
    NoDefault() = delete;
    explicit NoDefault(int) {}
};

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

static_assert(!std::is_copy_constructible_v<vector<int>>);
static_assert(!std::is_copy_assignable_v<vector<int>>);
static_assert(!std::is_move_constructible_v<vector<int>>);
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
