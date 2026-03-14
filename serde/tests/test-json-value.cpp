#include <doctest/doctest.h>

import reflex.serde.json;

import std;

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
