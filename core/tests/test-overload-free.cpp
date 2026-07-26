#include <doctest/doctest.h>

#include <reflex/core.hpp>

#include <string>

namespace free_fns
{
  int         f(int) { return 1; }
  double      f(double) { return 2.0; }
  std::string f(std::string_view s) { return std::string{s} + "!"; }

  int only_double(double v) { return static_cast<int>(v) * 10; }

  // No single overload is better than the other for a call with two ints.
  int ambiguous(int, double) { return 1; }
  int ambiguous(double, int) { return 2; }

  int single(int v) { return v; }
} // namespace free_fns

int global_fn(int v)
{
  return v + 1000;
}

using namespace std::string_view_literals;

TEST_CASE("reflex::resolve: free functions")
{
  constexpr auto f = reflex::resolve<^^free_fns, "f">;

  SUBCASE("each overload is reachable")
  {
    CHECK_EQ(f(1), 1);
    CHECK_EQ(f(2.0), 2.0);
    CHECK_EQ(f("hello"sv), "hello!");
  }
  SUBCASE("the return type follows the selected overload")
  {
    static_assert(std::same_as<std::invoke_result_t<decltype(f), int>, int>);
    static_assert(std::same_as<std::invoke_result_t<decltype(f), double>, double>);
    static_assert(std::same_as<std::invoke_result_t<decltype(f), std::string_view>, std::string>);
  }
  SUBCASE("a converting match is accepted")
  {
    constexpr auto od = reflex::resolve<^^free_fns, "only_double">;
    CHECK_EQ(od(3), 30);
    static_assert(std::invocable<decltype(od), int>);
  }
  SUBCASE("a call matching nothing is not invocable")
  {
    static_assert(not std::invocable<decltype(f), void*>);
    static_assert(not std::invocable<decltype(f)>);
    static_assert(not std::invocable<decltype(f), int, int>);
  }
  SUBCASE("an ambiguous call is reported, not compiled")
  {
    constexpr auto amb = reflex::resolve<^^free_fns, "ambiguous">;
    static_assert(not std::invocable<decltype(amb), int, int>);
    static_assert(std::invocable<decltype(amb), int, double>);
    CHECK_EQ(amb(1, 2.0), 1);
    CHECK_EQ(amb(1.0, 2), 2);
  }
  SUBCASE("a name with one overload still resolves")
  {
    constexpr auto one = reflex::resolve<^^free_fns, "single">;
    CHECK_EQ(one(7), 7);
    static_assert(not std::invocable<decltype(one), void*>);
  }
  SUBCASE("the global namespace is a scope like any other")
  {
    constexpr auto g = reflex::resolve<^^::, "global_fn">;
    CHECK_EQ(g(1), 1001);
  }
}

TEST_CASE("reflex::overloads_of: free functions")
{
  SUBCASE("every overload of the name is listed")
  {
    constexpr auto count = reflex::overloads_of(^^free_fns, "f").size();
    static_assert(count == 3);
  }
  SUBCASE("a candidate carries its arity, return type and parameter types")
  {
    constexpr auto single = [] consteval {
      auto all = reflex::overloads_of(^^free_fns, "single");
      return all.front();
    }();
    static_assert(single.arity == 1);
    static_assert(single.return_type() == ^^int);
    static_assert(single.parameter_types().size() == 1);
    static_assert(single.parameter_types().front() == ^^int);
    static_assert(single.function == ^^free_fns::single);
  }
  SUBCASE("an unknown name yields no candidates")
  {
    static_assert(reflex::overloads_of(^^free_fns, "nope").empty());
  }
}
