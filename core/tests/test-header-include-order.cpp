// Header-only regression test, and the include order below is the point of it.
//
// hash.hpp, parse.hpp and enum.hpp each declare tag_invoke overloads directly
// in namespace reflex, next to the CPO object of the same name that
// tag_invoke.hpp brings in with a using-directive. Any library code calling a
// bare `tag_invoke(...)` from inside reflex then sees a variable and a function
// set under one name, which is ambiguous, and whether it compiles depends on
// which header was included first. Including hash.hpp before parse.hpp used to
// fail exactly that way.
//
// This file deliberately does not import a module: only the header-only path
// can see the problem, since a module build never exposes a user-chosen order.
#include <reflex/hash.hpp>
#include <reflex/parse.hpp>
#include <reflex/enum.hpp>

#include <doctest/doctest.h>

using namespace reflex;

namespace
{
  enum class [[= derive(Parse)]] colour
  {
    red,
    green,
    blue
  };
}

TEST_CASE("reflex: hash.hpp and parse.hpp coexist in one translation unit")
{
  SUBCASE("parse still dispatches")
  {
    const auto n = parse<int>("42");
    REQUIRE(n.has_value());
    CHECK_EQ(n.value(), 42);

    const auto d = parse<double>("2.5rest");
    REQUIRE(d.has_value());
    CHECK_EQ(d.value(), 2.5);
    CHECK_EQ(*d.end(), 'r');

    CHECK_FALSE(parse_strict<int>("42rest").has_value());
  }
  SUBCASE("hash still dispatches")
  {
    CHECK_EQ(reflex::hash(std::string_view{"abc"}), reflex::hash(std::string_view{"abc"}));
    CHECK_NE(reflex::hash(std::string_view{"abc"}), reflex::hash(std::string_view{"abd"}));
    CHECK_EQ(reflex::hash(colour::green), reflex::hash(colour::green));
  }
  SUBCASE("enum parsing still dispatches")
  {
    const auto c = parse<colour>("blue");
    REQUIRE(c.has_value());
    CHECK(c.value() == colour::blue);
    CHECK_EQ(enum_value_name(colour::green), "green");
  }
}
