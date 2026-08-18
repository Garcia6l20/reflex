#include <doctest/doctest.h>

import reflex.core;
import std;

namespace probe
{
  struct flat
  {
    int  first  = 0;
    long second = 0;
  };

  struct l0
  {
    int alpha = 0;
  };

  struct l1 : l0
  {
    long beta = 0;
  };

  struct l2 : l1
  {
    double gamma = 0;
  };

  struct shadow_base
  {
    int value = 0;
  };

  struct shadow_derived : shadow_base
  {
    double value = 0;
  };

  struct hidden
  {
  private:
    int secret = 0;
  };

  struct exposed : hidden
  {
    int visible = 0;
  };

  struct left
  {
    int shared = 0;
  };

  struct right
  {
    double shared = 0;
  };

  struct joined : left, right
  {
  };
}

using reflex::meta::member_named;

TEST_CASE("reflex::meta::member_named: a single class")
{
  static_assert(member_named(^^probe::flat, "first") != reflex::meta::null);
  static_assert(std::meta::type_of(member_named(^^probe::flat, "second")) == ^^long);
  static_assert(member_named(^^probe::flat, "missing") == reflex::meta::null);
}

TEST_CASE("reflex::meta::member_named: the default stays non-recursive")
{
  static_assert(member_named(^^probe::l1, "alpha") == reflex::meta::null);
  static_assert(member_named(^^probe::l2, "beta") == reflex::meta::null);
  static_assert(member_named(^^probe::l2, "gamma") != reflex::meta::null);
}

TEST_CASE("reflex::meta::member_named: a two-level hierarchy")
{
  static constexpr auto alpha = member_named(^^probe::l1, "alpha", true);

  static_assert(alpha != reflex::meta::null);
  static_assert(std::meta::type_of(alpha) == ^^int);
  static_assert(std::meta::parent_of(alpha) == ^^probe::l0);
  static_assert(member_named(^^probe::l1, "beta", true) != reflex::meta::null);
}

TEST_CASE("reflex::meta::member_named: a three-level hierarchy")
{
  static_assert(std::meta::parent_of(member_named(^^probe::l2, "alpha", true)) == ^^probe::l0);
  static_assert(std::meta::parent_of(member_named(^^probe::l2, "beta", true)) == ^^probe::l1);
  static_assert(std::meta::parent_of(member_named(^^probe::l2, "gamma", true)) == ^^probe::l2);
  static_assert(member_named(^^probe::l2, "delta", true) == reflex::meta::null);
}

TEST_CASE("reflex::meta::member_named: a derived member shadows a base member")
{
  static constexpr auto value = member_named(^^probe::shadow_derived, "value", true);

  static_assert(std::meta::type_of(value) == ^^double);
  static_assert(std::meta::parent_of(value) == ^^probe::shadow_derived);
  static_assert(std::meta::type_of(member_named(^^probe::shadow_base, "value", true)) == ^^int);
}

TEST_CASE("reflex::meta::member_named: the access context governs a base member")
{
  static constexpr auto unchecked = std::meta::access_context::unchecked();

  static_assert(member_named(^^probe::exposed, "secret", unchecked, true) != reflex::meta::null);
  static_assert(std::meta::parent_of(member_named(^^probe::exposed, "secret", unchecked, true))
                == ^^probe::hidden);
  static_assert(member_named(^^probe::exposed, "secret", true) == reflex::meta::null);
  static_assert(member_named(^^probe::exposed, "visible", true) != reflex::meta::null);
}

TEST_CASE("reflex::meta::member_named: the first-declared base wins")
{
  static constexpr auto shared = member_named(^^probe::joined, "shared", true);

  static_assert(std::meta::type_of(shared) == ^^int);
  static_assert(std::meta::parent_of(shared) == ^^probe::left);
}

TEST_CASE("reflex::meta::member_named: a recursive lookup on a namespace")
{
  static_assert(member_named(^^probe, "flat", true) != reflex::meta::null);
  static_assert(member_named(^^probe, "nope", true) == reflex::meta::null);
}
