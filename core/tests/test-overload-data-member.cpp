#include <doctest/doctest.h>

#include <reflex/core.hpp>

#include <functional>
#include <string>

namespace
{
  int doubled(int v)
  {
    return v * 2;
  }

  struct handlers
  {
    // a function pointer
    int (*by_pointer)(int) = &doubled;

    // a function reference
    int (&by_reference)(int) = doubled;

    // a class type with a call operator
    std::function<std::string(std::string_view)> by_function =
        [](std::string_view s) { return std::string{s} + "!"; };

    // a lambda with concrete parameters
    decltype([](int a, int b) { return a + b; }) by_lambda{};

    // not callable, must stay out of the set
    int plain = 3;
  };

  // a call operator that is a template has no parameter types until it is
  // substituted, the same reason a function template is not a candidate
  struct generic
  {
    decltype([](auto v) { return v; }) any{};
  };
} // namespace

using namespace std::string_view_literals;

TEST_CASE("reflex::resolve: invocable data members")
{
  handlers h{};

  SUBCASE("a function pointer member is callable through the object")
  {
    CHECK_EQ(reflex::resolve<^^handlers, "by_pointer">(h, 4), 8);
    static_assert(
        std::same_as<std::invoke_result_t<decltype(reflex::resolve<^^handlers, "by_pointer">),
                                          handlers&, int>,
                     int>);
  }
  SUBCASE("a function reference member is callable too")
  {
    CHECK_EQ(reflex::resolve<^^handlers, "by_reference">(h, 5), 10);
  }
  SUBCASE("a class type with a call operator is callable")
  {
    CHECK_EQ(reflex::resolve<^^handlers, "by_function">(h, "hi"sv), "hi!");
  }
  SUBCASE("a lambda member is callable")
  {
    CHECK_EQ(reflex::resolve<^^handlers, "by_lambda">(h, 2, 3), 5);
  }
  SUBCASE("a const object reaches a callable member")
  {
    const handlers ch{};
    CHECK_EQ(reflex::resolve<^^handlers, "by_pointer">(ch, 4), 8);
  }
  SUBCASE("a call matching the stored signature is required")
  {
    constexpr auto ptr = reflex::resolve<^^handlers, "by_pointer">;
    static_assert(std::invocable<decltype(ptr), handlers&, int>);
    static_assert(not std::invocable<decltype(ptr), handlers&, void*>);
    static_assert(not std::invocable<decltype(ptr), handlers&>);
  }
  SUBCASE("a data member that is not callable is not in the set")
  {
    constexpr auto plain = reflex::resolve<^^handlers, "plain">;
    static_assert(not std::invocable<decltype(plain), handlers&>);
    static_assert(not std::invocable<decltype(plain), handlers&, int>);
  }
  SUBCASE("a templated call operator is not a candidate")
  {
    constexpr auto any = reflex::resolve<^^generic, "any">;
    static_assert(not std::invocable<decltype(any), generic&, int>);
  }
}

TEST_CASE("reflex::overloads_of: invocable data members")
{
  SUBCASE("a callable data member is listed with the signature it stores")
  {
    constexpr auto one = [] consteval {
      return reflex::overloads_of(^^handlers, "by_pointer").front();
    }();
    static_assert(one.arity == 1);
    static_assert(one.return_type() == ^^int);
    static_assert(one.parameter_types().front() == ^^int);
    static_assert(one.function == ^^handlers::by_pointer);
  }
  SUBCASE("a data member that is not callable yields no candidate")
  {
    static_assert(reflex::overloads_of(^^handlers, "plain").empty());
    static_assert(reflex::overloads_of(^^generic, "any").empty());
  }
}
