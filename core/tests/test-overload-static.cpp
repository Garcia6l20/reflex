#include <doctest/doctest.h>

#include <reflex/core.hpp>

#include <string>

namespace
{
  struct factory
  {
    int seed = 5;

    // static only, several overloads
    static int    make(int v) { return v * 2; }
    static double make(double v) { return v + 0.5; }

    // one name carrying both a static and an instance overload
    static int count() { return 100; }
    int        count(int extra) const { return seed + extra; }

    // static with a defaulted parameter
    static int scaled(int v, int mul = 3) { return v * mul; }
  };
} // namespace

TEST_CASE("reflex::resolve: static member functions")
{
  SUBCASE("a static member is callable without an object")
  {
    constexpr auto make = reflex::resolve<^^factory, "make">;
    CHECK_EQ(make(3), 6);
    CHECK_EQ(make(3.0), 3.5);
    static_assert(std::same_as<std::invoke_result_t<decltype(make), int>, int>);
    static_assert(std::same_as<std::invoke_result_t<decltype(make), double>, double>);
  }
  SUBCASE("a static member is also callable through an object")
  {
    factory f{};
    CHECK_EQ(reflex::resolve<^^factory, "make">(f, 3), 6);
  }
  SUBCASE("a defaulted parameter works on a static member too")
  {
    constexpr auto scaled = reflex::resolve<^^factory, "scaled">;
    CHECK_EQ(scaled(4), 12);
    CHECK_EQ(scaled(4, 5), 20);
  }
  SUBCASE("a name carrying both reaches each through its own call")
  {
    constexpr auto count = reflex::resolve<^^factory, "count">;
    const factory  f{};
    CHECK_EQ(count(), 100);
    CHECK_EQ(count(f, 2), 7);
  }
  SUBCASE("a call matching no static member is not invocable without an object")
  {
    constexpr auto make = reflex::resolve<^^factory, "make">;
    static_assert(not std::invocable<decltype(make), void*>);
    static_assert(not std::invocable<decltype(make)>);
  }
  SUBCASE("an instance member is not reachable without an object")
  {
    constexpr auto count = reflex::resolve<^^factory, "count">;
    static_assert(not std::invocable<decltype(count), int>);
  }
}

TEST_CASE("reflex::overloads_of: static member functions")
{
  SUBCASE("static and instance overloads are listed under the same name")
  {
    constexpr auto all = [] consteval {
      return reflex::overloads_of(^^factory, "count");
    };
    static_assert(all().size() == 2);
    static_assert(std::meta::is_static_member(all()[0].function));
    static_assert(not std::meta::is_static_member(all()[1].function));
  }
}
