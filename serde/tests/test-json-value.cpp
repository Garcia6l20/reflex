#include <doctest/doctest.h>

#include <reflex/serde/json.hpp>

using namespace reflex;
using namespace reflex::serde;

TEST_CASE("serde::json::value: formattable")
{
  SUBCASE("null")
  {
    auto result = std::format("{}", json::value{json::null});
    CHECK(result == "null");
  }
}
