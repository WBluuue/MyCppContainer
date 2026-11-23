#include<catch2/catch_test_macros.hpp>
#include<mycppcontainer/config.hpp>

TEST_CASE("Library version is initialized","[smoke]")
{
    REQUIRE(mycppcontainer::version_major==0);
    CHECK(mycppcontainer::version_major>=0);
}