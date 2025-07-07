#include <reflex/named_tuple.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace reflex;

TEST_CASE("make_named_tuple explicit")
{
  constexpr auto t = make_named_tuple<"hello", "world">(1, 2);
  STATIC_REQUIRE(t.hello == 1);
  STATIC_REQUIRE(t.get<"hello">() == 1);
  STATIC_REQUIRE(get<"hello">(t) == 1);
  STATIC_REQUIRE(t.world == 2);
  STATIC_REQUIRE(t.get<"world">() == 2);
  STATIC_REQUIRE(get<"world">(t) == 2);

  std::println("============= all members =============");
  constexpr auto T = type_of(^^t);
  template for(constexpr auto I : define_static_array(meta::members_of_r(T, meta::access_context::current())))
  {
    println("{}", display_string_of(I));
  }

  std::println("============= nsd members =============");
  template for(constexpr auto I : define_static_array(meta::nonstatic_data_members_of_r(T,
  meta::access_context::current())))
  {
    println("{}", display_string_of(I));
  }

  constexpr auto tt = to_tuple(t);
  std::println("============= std::tuple values =============");
  template for (constexpr auto v : tt) {
    std::println("{}", v);
  }
}

TEST_CASE("make_named_tuple from struc") {
  struct test_s {
    float f;
    int i;
  };
  constexpr auto t = make_named_tuple(test_s{42.2f, 42});
  STATIC_REQUIRE(t.f == 42.2f);
  STATIC_REQUIRE(t.get<"f">() == 42.2f);
  STATIC_REQUIRE(get<"f">(t) == 42.2f);
  STATIC_REQUIRE(t.i == 42);
  STATIC_REQUIRE(t.get<"i">() == 42);
  STATIC_REQUIRE(get<"i">(t) == 42);
}

TEST_CASE("non-constexpr to_tuple")
{
  struct test_s
  {
    float f;
    int   i;
  };
  auto t = make_named_tuple(test_s{42.2f, 42});
  REQUIRE(t.f == 42.2f);
  REQUIRE(t.get<"f">() == 42.2f);
  REQUIRE(get<"f">(t) == 42.2f);
  REQUIRE(t.i == 42);
  REQUIRE(t.get<"i">() == 42);
  REQUIRE(get<"i">(t) == 42);

  auto tt = to_tuple(t);
}
