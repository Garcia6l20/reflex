#include <doctest/doctest.h>

import reflex.core;
import reflex.poly;
import reflex.serde;

import std;

using namespace reflex;
using namespace reflex::poly;
using namespace reflex::serde;
using namespace std::string_literals;

using value  = var<bool, double, std::string>;
using array  = value::arr_type;
using object = value::obj_type;

TEST_CASE("reflex::serde::object_visit: var")
{
  SUBCASE("simple")
  {
    object obj = {
        {"name", "alice"s},
        {"age",  30      }
    };
    bool visited = false;
    object_visit("age", obj, [&visited](auto const& value) {
      if constexpr(decays_to_c<decltype(value), double>)
      {
        CHECK(value == 30);
        visited = true;
      }
      else
      {
        FAIL("wrong type");
      }
    });
    CHECK(visited);
  }
  SUBCASE("recursive")
  {
    object obj = {
        {"alice",
         object{
             {"age", 30},
             {"address", object{{"city", "Wonderland"s}, {"zip", 12345}}},
         }},
        {"bob",
         object{
             {"age", 66},
             {"address", object{{"city", "Badlands"s}, {"zip", 666}}},
         }},
    };
    {
      bool visited = false;
      object_visit("alice.age", obj, [&visited](auto const& value) {
        if constexpr(decays_to_c<decltype(value), double>)
        {
          std::println("Alice's age: {}", value);
          CHECK(value == 30);
          visited = true;
        }
        else
        {
          FAIL("wrong type");
        }
      });
      CHECK(visited);
    }
    {
      bool visited = false;
      object_visit("bob.address.city", obj, [&visited](auto const& value) {
        if constexpr(decays_to_c<decltype(value), std::string>)
        {
          std::println("Bob's city: {}", value);
          CHECK(value == "Badlands");
          visited = true;
        }
        else
        {
          FAIL("wrong type");
        }
      });
      CHECK(visited);
    }
  }
}

struct address
{
  std::string city;
  int         zip;
};

struct person
{
  std::string name;
  int         age;
  address     addr;
};

TEST_CASE("reflex::serde::object_visit: aggregates")
{
  person alice{
      .name = "alice"s,
      .age  = 30,
      .addr =
          {
                 .city = "Wonderland"s,
                 .zip  = 12345,
                 },
  };
  SUBCASE("simple")
  {
    bool visited = false;
    object_visit("age", alice, [&visited](auto const& value) {
      if constexpr(decay(type_of(^^value)) == ^^int)
      {
        CHECK(value == 30);
        visited = true;
      }
      else
      {
        FAIL("Expected int");
      }
    });
    CHECK(visited);
  }
  SUBCASE("recursive")
  {
    {
      bool visited = false;
      object_visit("addr.city", alice, [&visited](auto const& value) {
        if constexpr(decay(type_of(^^value)) == dealias(^^std::string))
        {
          // std::println("Alice's city: {}", value);
          CHECK(value == "Wonderland");
          visited = true;
        }
        else
        {
          FAIL("Expected string");
        }
      });
      CHECK(visited);
    }
  }
}

TEST_CASE("reflex::serde::object_visit: var with aggregates")
{
  person alice{
      .name = "alice"s,
      .age  = 30,
      .addr =
          {
                 .city = "Wonderland"s,
                 .zip  = 12345,
                 },
  };
  person bob{
      .name = "bob"s,
      .age  = 66,
      .addr =
          {
                 .city = "Badlands"s,
                 .zip  = 000,
                 },
  };

  obj<int, bool, double, person&> obj = {
      {"alice", std::ref(alice)},
      {"bob",   std::ref(bob)  },
  };

  SUBCASE("simple")
  {
    SUBCASE("alice")
    {
      bool visited = false;
      object_visit("alice.age", obj, [&visited](auto const& value) {
        if constexpr(decay(type_of(^^value)) == ^^int)
        {
          CHECK(value == 30);
          visited = true;
        }
        else
        {
          FAIL("Expected int");
        }
      });
      CHECK(visited);
    }
    SUBCASE("bob - initial")
    {
      bool visited = false;
      object_visit("bob.addr.zip", obj, [&visited](auto const& value) {
        if constexpr(decay(type_of(^^value)) == ^^int)
        {
          CHECK(value == 000);
          visited = true;
        }
        else
        {
          FAIL("Expected int");
        }
      });
      CHECK(visited);
    }
    SUBCASE("bob - modified")
    {
      bob.addr.zip = 666;
      bool visited = false;
      object_visit("bob.addr.zip", obj, [&visited](auto const& value) {
        if constexpr(decay(type_of(^^value)) == ^^int)
        {
          CHECK(value == 666);
          visited = true;
        }
        else
        {
          FAIL("Expected int");
        }
      });
      CHECK(visited);
    }
  }
}

TEST_CASE("reflex::serde::object_visit: dotted key depth is bounded")
{
  // Keys reach object_visit straight from a document, so the segment count is
  // input-controlled. Past max_key_depth it has to be reported, not written
  // past the end of the fixed array.
  const auto dotted = [](std::size_t segments) {
    std::string key = "name";
    for(std::size_t i = 1; i < segments; ++i)
    {
      key += ".name";
    }
    return key;
  };

  person alice{
      .name = "alice"s,
      .age  = 30,
      .addr = {.city = "Wonderland"s, .zip = 12345},
  };

  SUBCASE("at the limit")
  {
    CHECK_NOTHROW(object_visit(dotted(max_key_depth), alice, [](auto const&) {}));
  }
  SUBCASE("past the limit")
  {
    CHECK_THROWS_AS(object_visit(dotted(max_key_depth + 1), alice, [](auto const&) {}),
                    std::runtime_error);
    CHECK_THROWS_AS(object_visit(dotted(200), alice, [](auto const&) {}), std::runtime_error);
  }
}

TEST_CASE("reflex::serde::object_visit_flat: a dot is part of the name")
{
  person alice{
      .name = "alice"s,
      .age  = 30,
      .addr = {.city = "Wonderland"s, .zip = 12345},
  };

  SUBCASE("object_visit walks the path")
  {
    bool visited = false;
    object_visit("addr.city", alice, [&visited](auto const& value) {
      if constexpr(decay(type_of(^^value)) == dealias(^^std::string))
      {
        CHECK(value == "Wonderland");
        visited = true;
      }
    });
    CHECK(visited);
  }
  SUBCASE("object_visit_flat looks for a member actually called that")
  {
    // A key read out of a document names one member. Splitting it would let
    // the document reach a member it never named, so the whole key is matched.
    CHECK_THROWS_AS(object_visit_flat("addr.city", alice, [](auto const&) {}), std::runtime_error);
  }
  SUBCASE("an undotted key is unaffected")
  {
    bool visited = false;
    object_visit_flat("age", alice, [&visited](auto const& value) {
      if constexpr(decay(type_of(^^value)) == ^^int)
      {
        CHECK(value == 30);
        visited = true;
      }
    });
    CHECK(visited);
  }
}

// The visitor picks its key-matching strategy on member count and name length,
// so each strategy needs a shape that reaches it. Below the threshold it
// compares names in a chain, above it rejects on a hashed key when every name
// fits a machine word and on a length-and-first-word pair when one does not.
struct narrow_shape
{
  int a0, a1, a2;
};

struct wide_short_shape
{
  int s00, s01, s02, s03, s04, s05, s06, s07, s08, s09;
  int s10, s11, s12, s13, s14, s15, s16, s17, s18, s19;
  int s20, s21, s22, s23, s24, s25;
};

struct wide_long_shape
{
  int identifier_number, identifier_suffix, measurement_value, measurement_scale;
  int calibration_first, calibration_other, revisionlog_first, revisionlog_other;
  int adjustments_first, adjustments_other, corrections_first, corrections_other;
  int transitions_first, transitions_other, validations_first, validations_other;
  int assignments_first, assignments_other, conversions_first, conversions_other;
  int deviations0_first, deviations0_other, extensions0_first, extensions0_other;
  int formations0_first, formations0_other;
};

// Renaming makes a member carry two names, which every strategy has to keep
// matching, and the rename is what pushes this shape onto the long-name path.
struct wide_renamed_shape
{
  [[= serde::rename{"a_rather_long_replacement_name"}]] int r00;
  int t01, t02, t03, t04, t05, t06, t07, t08, t09;
  int t10, t11, t12, t13, t14, t15, t16, t17, t18, t19;
  int t20, t21, t22, t23;
};

// Pin which strategy each shape reaches, so a threshold change cannot quietly
// leave a path untested.
static_assert(object_visitor<narrow_shape>::__members().size() < serde::detail::wide_object_threshold);
static_assert(object_visitor<wide_short_shape>::__members().size() >= serde::detail::wide_object_threshold);
static_assert(not object_visitor<wide_short_shape>::__any_long_name());
static_assert(object_visitor<wide_long_shape>::__members().size() >= serde::detail::wide_object_threshold);
static_assert(object_visitor<wide_long_shape>::__any_long_name());
static_assert(object_visitor<wide_renamed_shape>::__members().size() >= serde::detail::wide_object_threshold);
static_assert(object_visitor<wide_renamed_shape>::__any_long_name());

TEST_CASE("reflex::serde::object_visit: every dispatch strategy matches the same way")
{
  const auto reads = [](auto& shape, std::string_view key, int expected) {
    bool visited = false;
    object_visit_flat(key, shape, [&](auto const& value) {
      if constexpr(decay(type_of(^^value)) == ^^int)
      {
        CHECK_EQ(value, expected);
        visited = true;
      }
    });
    return visited;
  };

  SUBCASE("narrow, name chain")
  {
    narrow_shape v{.a0 = 1, .a1 = 2, .a2 = 3};
    CHECK(reads(v, "a0", 1));
    CHECK(reads(v, "a2", 3));
    CHECK_THROWS_AS(object_visit_flat("nope", v, [](auto const&) {}), std::runtime_error);
  }
  SUBCASE("wide, every name fits a word")
  {
    wide_short_shape v{};
    v.s00 = 10;
    v.s25 = 26;
    CHECK(reads(v, "s00", 10));
    CHECK(reads(v, "s25", 26));
    CHECK(reads(v, "s13", 0));
    CHECK_THROWS_AS(object_visit_flat("s26", v, [](auto const&) {}), std::runtime_error);
    // A key sharing a prefix with a member but not its length must not match.
    CHECK_THROWS_AS(object_visit_flat("s0", v, [](auto const&) {}), std::runtime_error);
  }
  SUBCASE("wide, names run past a word")
  {
    wide_long_shape v{};
    v.identifier_number = 7;
    v.formations0_other = 9;
    CHECK(reads(v, "identifier_number", 7));
    CHECK(reads(v, "formations0_other", 9));
    // Shares its first eight bytes with identifier_number, so only the full
    // comparison separates them.
    CHECK(reads(v, "identifier_suffix", 0));
    CHECK_THROWS_AS(object_visit_flat("identifier_missing", v, [](auto const&) {}),
                    std::runtime_error);
  }
  SUBCASE("wide, a renamed member answers to both names")
  {
    wide_renamed_shape v{};
    v.r00 = 5;
    v.t23 = 24;
    CHECK(reads(v, "a_rather_long_replacement_name", 5));
    CHECK(reads(v, "r00", 5));
    CHECK(reads(v, "t23", 24));
    CHECK_THROWS_AS(object_visit_flat("a_rather_long_replacement", v, [](auto const&) {}),
                    std::runtime_error);
  }
}
