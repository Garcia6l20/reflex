#include <doctest/doctest.h>

import reflex.core;
import std;

TEST_CASE("reflex::natural_less")
{
  using reflex::natural_less;

  SUBCASE("digit runs compare as numbers")
  {
    CHECK(natural_less("PA2", "PA10"));
    CHECK(not natural_less("PA10", "PA2"));
    CHECK(not natural_less("PA2", "PA2"));
    CHECK(natural_less("x9y", "x10y"));
  }

  SUBCASE("leading zeros are skipped, so a01 and a1 are the same number")
  {
    CHECK(not natural_less("a01", "a1"));
    CHECK(not natural_less("a1", "a01"));
    CHECK(natural_less("a01", "a2"));
  }

  SUBCASE("everything else compares case-insensitively")
  {
    CHECK(natural_less("x2", "X10"));
    CHECK(not natural_less("X10", "x2"));
    CHECK(natural_less("abc", "ABD"));
  }

  SUBCASE("a prefix comes first")
  {
    CHECK(natural_less("ab", "abc"));
    CHECK(not natural_less("abc", "ab"));
    CHECK(natural_less("", "a"));
    CHECK(not natural_less("", ""));
  }

  // the constexpr path
  static_assert(natural_less("PA2", "PA10"));
  static_assert(not natural_less("PA10", "PA2"));
}

TEST_CASE("reflex::trim")
{
  using namespace reflex;

  CHECK(trim("  x  ") == "x");
  CHECK(ltrim("  x  ") == "x  ");
  CHECK(rtrim("  x  ") == "  x");
  CHECK(trim("   ") == "");

  static_assert(trim(" a ") == "a");
}
