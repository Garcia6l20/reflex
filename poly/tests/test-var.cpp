#include <doctest/doctest.h>

#include <reflex/core.hpp>
#include <reflex/poly.hpp>

using namespace reflex;
using namespace reflex::poly;
using namespace std::string_literals;

using value  = var<bool, double, std::string>;
using array  = value::arr_type;
using object = value::obj_type;

TEST_CASE("reflex::poly::var: constructs")
{
  SUBCASE("null")
  {
    value v = null;
    CHECK(v.is_null());
    CHECK(v == null);
  }

  SUBCASE("boolean")
  {
    value t = true;
    value f = false;
    CHECK(t.template is<bool>());
    CHECK(t == true);
    CHECK(f == false);
  }

  SUBCASE("number - double")
  {
    value v = 3.14;
    CHECK(v.template is<double>());
    CHECK(v == 3.14);
  }

  SUBCASE("number - integral promotion")
  {
    value v = 42; // int => double
    CHECK(v.template is<double>());
    CHECK(v == 42);

    value u = std::size_t{7};
    CHECK(u.template is<double>());
    CHECK(u == 7);
  }

  SUBCASE("string")
  {
    value v = "hello"s;
    CHECK(v.template is<std::string>());
    CHECK(v == "hello");
  }

  SUBCASE("array - initializer list")
  {
    array arr{1, 2.0, "three"s};
    value v = arr;
    CHECK(v.template is<array>());
    CHECK(v.size() == 3);
  }

  SUBCASE("object - initializer list")
  {
    value v = {
        {"name", "alice"s},
        {"age",  30      }
    };
    CHECK(v.template is<object>());
    CHECK(v.size() == 2);
  }
}

TEST_CASE("reflex::poly::var: as<T> and get<T>")
{
  SUBCASE("as<T> const")
  {
    value v = std::string{"world"};
    CHECK(v.as<string>() == "world");
  }

  SUBCASE("as<T> mutable")
  {
    value v = std::string{"foo"};
    v.as<string>() += "bar";
    CHECK(v == "foobar");
  }

  SUBCASE("get<T> returns value when type matches")
  {
    value v   = 1.5;
    auto  opt = v.get<double>();
    REQUIRE(opt.has_value());
    CHECK(opt.value() == doctest::Approx(1.5));
  }

  SUBCASE("get<T> returns nullopt when type mismatches")
  {
    value v   = true;
    auto  opt = v.get<double>();
    CHECK_FALSE(opt.has_value());
  }
}

TEST_CASE("reflex::poly::var: array access")
{
  value v = array{10, 20, 30};

  SUBCASE("operator[] by index")
  {
    CHECK(v[0] == 10);
    CHECK(v[1] == 20);
    CHECK(v[2] == 30);
  }

  SUBCASE("push_back")
  {
    v.push_back(40);
    CHECK(v.size() == 4);
    CHECK(v[3] == 40);
  }

  SUBCASE("size / empty")
  {
    CHECK(v.size() == 3);
    CHECK_FALSE(v.empty());

    value empty = array{};
    CHECK(empty.empty());
  }
}

TEST_CASE("reflex::poly::var: object access")
{
  value v = {
      {"name",   "bob"s},
      {"score",  99    },
      {"active", true  }
  };

  SUBCASE("operator[] single key")
  {
    CHECK(v["name"] == "bob");
    CHECK(v["score"] == 99);
    CHECK(v["active"] == true);
  }

  SUBCASE("missing key returns null")
  {
    CHECK(v["missing"] == null);
  }

  SUBCASE("contains")
  {
    CHECK(v.contains("name"));
    CHECK_FALSE(v.contains("nope"));
  }

  SUBCASE("size / empty")
  {
    CHECK(v.size() == 3);
    CHECK_FALSE(v.empty());
  }

  SUBCASE("mutable operator[]")
  {
    v["score"].as<double>() = 100.0;
    CHECK(v["score"] == 100);
  }
}

TEST_CASE("reflex::poly::var: dotted path access")
{
  value v = {
      {"user",
       value{{"name", "carol"s}, {"address", value{{"city", "Paris"s}, {"country", "France"s}}}}},
      {"score", 42                                                                              }
  };

  SUBCASE("single level")
  {
    CHECK(v["score"] == 42);
  }

  SUBCASE("two levels")
  {
    CHECK(v["user.name"] == "carol");
  }

  SUBCASE("three levels")
  {
    CHECK(v["user.address.city"] == "Paris");
    CHECK(v["user.address.country"] == "France");
  }

  SUBCASE("missing intermediate returns null")
  {
    CHECK(v["user.nope.city"] == null);
  }

  SUBCASE("missing leaf returns null")
  {
    CHECK(v["user.age"] == null);
  }

  SUBCASE("contains with dotted path")
  {
    CHECK(v.contains("user.name"));
    CHECK(v.contains("user.address.city"));
    CHECK_FALSE(v.contains("user.phone"));
    CHECK_FALSE(v.contains("user.address.zip"));
  }
}

TEST_CASE("reflex::poly::var: merge")
{
  value v = {
      {"a", 1},
      {"b", 2}
  };
  object extra = {
      {"b", 99},
      {"c", 3 }
  };

  v.merge(extra);

  CHECK(v["a"] == 1);
  CHECK(v["b"] == 99); // overwritten
  CHECK(v["c"] == 3);  // inserted
}

TEST_CASE("reflex::poly::var: visit")
{
  SUBCASE("dispatches to correct type")
  {
    value v      = std::string{"hi"};
    bool  called = false;
    v.visit(
        match{
            [&](std::string const& s) {
              called = true;
              CHECK(s == "hi");
            },
            [](auto const&) { FAIL("wrong type"); },
        });
    CHECK(called);
  }

  SUBCASE("visit number")
  {
    value v = 7.0;
    v.visit(
        match{
            [](double n) { CHECK(n == doctest::Approx(7.0)); },
            [](auto const&) { FAIL("wrong type"); },
        });
  }
}

TEST_CASE("reflex::poly::var: size and empty for null/scalar")
{
  CHECK(value{null}.size() == 0);
  CHECK(value{null}.empty() == true);
  CHECK(value{42}.size() == 1);
  CHECK(value{42}.empty() == false);
  CHECK(value{"ab"s}.size() == 2); // string char count
}

TEST_CASE("reflex::poly::var: formattable")
{
  value v = {
      {"user",
       value{{"name", "carol"s}, {"address", value{{"city", "Paris"s}, {"country", "France"s}}}}},
      {"score", 42                                                                              },
      {"null",  null                                                                            },
  };
  std::println("value: {}", v);
}

TEST_CASE("reflex::poly::var: references")
{
  using value_with_refs  = var<bool, int, bool&, int&>;
  bool            a_bool = false;
  value_with_refs v      = std::ref(a_bool);
  visit(
      match{
          [&](bool& b) {
            CHECK(&b == &a_bool);
            b = true;
          },
          [](auto const&) { FAIL("wrong type"); },
      },
      v);
  visit(
      match{
          [&](bool const& b) {
            CHECK(&b == &a_bool);
            CHECK(b == true);
          },
          [](auto const&) { FAIL("wrong type"); },
      },
      v);
  CHECK(v == true);
  CHECK(v.template is<bool&>());
  CHECK(v == std::ref(a_bool));
  CHECK(a_bool == true);
  CHECK(v.template as<bool&>() == true);
  CHECK(v.template get<bool&>().value() == true);
  a_bool = false;
  CHECK(v == false);
  std::println("value with refs: {}", v);
}

struct aggregate1
{
  int         a;
  std::string b;
};

TEST_CASE("reflex::poly::var: ref to aggregates")
{
  using value_with_refs = var<bool, int, aggregate1&>;
  auto            a     = aggregate1{42, "the response to everything"};
  value_with_refs v     = std::ref(a);
  visit(
      match{
          [&](aggregate1& agg) {
            CHECK(&agg == &a);
            CHECK(agg.a == 42);
            CHECK(agg.b == "the response to everything");
          },
          [](auto const&) { FAIL("wrong type"); },
      },
      v);
  std::println("value with ref to aggregate: {}", v);
}
