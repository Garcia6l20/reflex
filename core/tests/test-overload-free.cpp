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

  int one_default(double v, int mul = 3) { return static_cast<int>(v) * mul; }

  std::string two_defaults(std::string_view head, int n = 2, char sep = '-')
  {
    return std::string{head} + sep + std::to_string(n);
  }

  int all_defaults(int a = 1, int b = 2) { return a * 10 + b; }
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

TEST_CASE("reflex::resolve: default arguments")
{
  SUBCASE("a defaulted parameter may be omitted")
  {
    constexpr auto od = reflex::resolve<^^free_fns, "one_default">;
    CHECK_EQ(od(2.0), 6);
    CHECK_EQ(od(2.0, 5), 10);
    static_assert(std::invocable<decltype(od), double>);
    static_assert(std::invocable<decltype(od), double, int>);
    static_assert(not std::invocable<decltype(od)>);
    static_assert(not std::invocable<decltype(od), double, int, int>);
  }
  SUBCASE("two defaults give three arities")
  {
    constexpr auto td = reflex::resolve<^^free_fns, "two_defaults">;
    CHECK_EQ(td("a"sv), "a-2");
    CHECK_EQ(td("a"sv, 7), "a-7");
    CHECK_EQ(td("a"sv, 7, '+'), "a+7");
    static_assert(not std::invocable<decltype(td)>);
  }
  SUBCASE("a fully defaulted function is invocable with no argument")
  {
    constexpr auto ad = reflex::resolve<^^free_fns, "all_defaults">;
    CHECK_EQ(ad(), 12);
    CHECK_EQ(ad(3), 32);
    CHECK_EQ(ad(3, 4), 34);
  }
  SUBCASE("the return type is the same whichever arity is selected")
  {
    constexpr auto td = reflex::resolve<^^free_fns, "two_defaults">;
    static_assert(std::same_as<std::invoke_result_t<decltype(td), std::string_view>, std::string>);
    static_assert(
        std::same_as<std::invoke_result_t<decltype(td), std::string_view, int>, std::string>);
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
  SUBCASE("a defaulted parameter yields one candidate per reachable arity")
  {
    constexpr auto arities = [] consteval {
      std::vector<std::size_t> out;
      for(auto o : reflex::overloads_of(^^free_fns, "two_defaults"))
      {
        out.push_back(o.arity);
      }
      return out;
    };
    static_assert(arities().size() == 3);
    static_assert(arities()[0] == 1);
    static_assert(arities()[1] == 2);
    static_assert(arities()[2] == 3);
  }
  SUBCASE("a candidate reports only the parameters it takes")
  {
    constexpr auto shortest = [] consteval {
      return reflex::overloads_of(^^free_fns, "one_default").front();
    }();
    static_assert(shortest.arity == 1);
    static_assert(shortest.parameter_types().size() == 1);
    static_assert(shortest.parameter_types().front() == ^^double);
  }
}
