#include <reflex/heapless/string.hpp>

#include <doctest/doctest.h>

using namespace reflex;
namespace hl = reflex::heapless;

TEST_CASE("reflex::heapless::string: constructs")
{
  SUBCASE("default")
  {
    hl::string<32> str;
    CHECK(str.empty());
  }
  SUBCASE("from literal")
  {
    hl::string<32> str = "hello";
    CHECK(str.size() == 5);
    CHECK(str == "hello");
  }
}

TEST_CASE("reflex::heapless::string: formattable")
{
    CHECK(std::format("hello {}", hl::string<32>{"world"}) == "hello world");
}
