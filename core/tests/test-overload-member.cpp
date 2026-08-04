#include <doctest/doctest.h>

#include <reflex/core.hpp>

#include <string>

namespace
{
  struct widget
  {
    // const and non const under one name
    int value() { return 1; }
    int value() const { return 2; }

    // mixed: one const overload, one taking an argument
    int act() const { return 42; }
    int act(int delta) { return 100 + delta; }

    // ref qualified
    int take() & { return 1; }
    int take() && { return 2; }

    // plain overloads, ranked on the argument alone
    int         convert(int) { return 10; }
    double      convert(double) const { return 20.5; }
    std::string convert(std::string_view s) const { return std::string{s} + "!"; }

    // a defaulted parameter, on a member this time
    int scaled(int v, int mul = 3) const { return v * mul; }

    // lvalue only
    int borrow() & { return 7; }

    int plain(int v) const { return v; }
  };
} // namespace

using namespace std::string_view_literals;

TEST_CASE("reflex::resolve: member functions")
{
  widget       w{};
  const widget cw{};

  SUBCASE("the object constness picks the overload")
  {
    CHECK_EQ(reflex::resolve<^^widget, "value">(w), 1);
    CHECK_EQ(reflex::resolve<^^widget, "value">(cw), 2);
  }
  SUBCASE("a const object reaches a const member of a mixed set")
  {
    CHECK_EQ(reflex::resolve<^^widget, "act">(cw), 42);
    CHECK_EQ(reflex::resolve<^^widget, "act">(w, 7), 107);
  }
  SUBCASE("the value category picks the ref qualified overload")
  {
    CHECK_EQ(reflex::resolve<^^widget, "take">(w), 1);
    CHECK_EQ(reflex::resolve<^^widget, "take">(std::move(w)), 2);
  }
  SUBCASE("arguments are ranked without the object interfering")
  {
    CHECK_EQ(reflex::resolve<^^widget, "convert">(w, 1), 10);
    CHECK_EQ(reflex::resolve<^^widget, "convert">(w, 2.0), 20.5);
    CHECK_EQ(reflex::resolve<^^widget, "convert">(w, "hi"sv), "hi!");
  }
  SUBCASE("a defaulted parameter may be omitted on a member too")
  {
    CHECK_EQ(reflex::resolve<^^widget, "scaled">(w, 4), 12);
    CHECK_EQ(reflex::resolve<^^widget, "scaled">(w, 4, 5), 20);
  }
  SUBCASE("a non const member is unreachable from a const object")
  {
    constexpr auto borrow = reflex::resolve<^^widget, "borrow">;
    static_assert(std::invocable<decltype(borrow), widget&>);
    static_assert(not std::invocable<decltype(borrow), const widget&>);
  }
  SUBCASE("an lvalue only member is unreachable from an rvalue")
  {
    constexpr auto borrow = reflex::resolve<^^widget, "borrow">;
    static_assert(not std::invocable<decltype(borrow), widget&&>);
  }
  SUBCASE("a call matching no member is not invocable")
  {
    constexpr auto plain = reflex::resolve<^^widget, "plain">;
    static_assert(std::invocable<decltype(plain), widget&, int>);
    static_assert(not std::invocable<decltype(plain), widget&, void*>);
    static_assert(not std::invocable<decltype(plain), widget&>);
    static_assert(not std::invocable<decltype(plain), widget&, int, int>);
  }
  SUBCASE("the return type follows the selected member")
  {
    constexpr auto convert = reflex::resolve<^^widget, "convert">;
    static_assert(std::same_as<std::invoke_result_t<decltype(convert), widget&, int>, int>);
    static_assert(std::same_as<std::invoke_result_t<decltype(convert), widget&, double>, double>);
    static_assert(
        std::same_as<std::invoke_result_t<decltype(convert), const widget&, double>, double>);
  }
}

TEST_CASE("reflex::overloads_of: member functions")
{
  SUBCASE("every overload of the name is listed, whatever it is qualified with")
  {
    constexpr auto count = reflex::overloads_of(^^widget, "convert").size();
    static_assert(count == 3);
  }
  SUBCASE("a member candidate reports the object it needs")
  {
    constexpr auto all = [] consteval {
      return reflex::overloads_of(^^widget, "value");
    };
    static_assert(all().size() == 2);
    static_assert(not std::meta::is_const(all()[0].function));
    static_assert(std::meta::is_const(all()[1].function));
  }
}
