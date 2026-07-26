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

struct opaque
{
  int x;
};

TEST_CASE("reflex::poly::var: equality with non-comparable alternatives")
{
  // std::vector<opaque>'s operator== is well-formed but instantiating it is not: the guard in
  // var::operator== must reject it without probing libstdc++'s unconstrained overload.
  using eq_value = var<bool, double, std::string, std::vector<opaque>>;

  eq_value v = 3.0;
  CHECK(v == 3);
  CHECK(v == 3.0);
  CHECK_FALSE(v == "x");

  eq_value s = "abc"s;
  CHECK(s == "abc");
  CHECK_FALSE(s == 3);

  eq_value o = std::vector<opaque>{{1}, {2}};
  CHECK_FALSE(o == 3);
  CHECK_FALSE(o == null);

  // comparing against the non-comparable type itself instantiates std::vector's unconstrained
  // operator== for every alternative, which only a deep comparability guard prevents
  CHECK_FALSE(v == std::vector<opaque>{{1}});
  CHECK_FALSE(o == std::vector<opaque>{{1}});
}

TEST_CASE("reflex::poly::var: references are opt-in")
{
  using value_with_refs = var<bool, int, std::string, std::string&, aggregate1&>;

  // an lvalue never binds by reference implicitly: a named local returned by value used to be
  // stored as a dangling std::string&
  auto make = []() -> value_with_refs {
    std::string local = "hello";
    return local;
  };
  auto v = make();
  CHECK(v.template is<std::string>());
  CHECK_FALSE(v.template is<std::string&>());
  CHECK(std::format("{}", v) == "hello");

  // a type with only a reference alternative is not implicitly bindable at all
  static_assert(not std::constructible_from<value_with_refs, aggregate1&>);

  auto a = aggregate1{1, "one"};
  auto r = value_with_refs{std::ref(a)};
  CHECK(r.template is<aggregate1&>());
  CHECK(&r.template as<aggregate1&>() == &a);
}

TEST_CASE("reflex::poly::var: std::ref without a reference alternative falls back to a copy")
{
  // the reference_wrapper ctor is constrained, so it drops out and the value alternative wins
  // instead of hard-erroring inside variant_type(std::string*)
  auto  s = "hello"s;
  value v = std::ref(s);
  CHECK(v.template is<std::string>());
  CHECK(v == "hello");
  s = "changed";
  CHECK(v == "hello"); // a copy, not a reference

  using ref_only = var<bool, int, aggregate1>;
  static_assert(not ref_only::can_hold<aggregate1&>());
  static_assert(
      not std::constructible_from<value, std::reference_wrapper<std::vector<std::string>>>);
}

TEST_CASE("reflex::poly::var: can_hold reports every alternative")
{
  static_assert(value::can_hold<bool>());
  static_assert(value::can_hold<double>());
  static_assert(value::can_hold<std::string>());
  static_assert(value::can_hold<null_t>());
  static_assert(value::can_hold<array>());
  static_assert(value::can_hold<object>());
  static_assert(value::can_hold<value&>());
  static_assert(value::can_hold<array&>());
  static_assert(value::can_hold<object&>());
  static_assert(not value::can_hold<char*>());
  static_assert(not value::can_hold<std::string&>());

  using value_with_refs = var<bool, int, aggregate1&>;
  static_assert(value_with_refs::can_hold<aggregate1&>());
  static_assert(not value_with_refs::can_hold<aggregate1>());
}
