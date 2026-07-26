#include <doctest/doctest.h>

#include <reflex/core.hpp>

#include <string>

namespace
{
  struct point
  {
    int         x = 0;
    std::string tag;

    point() = default;
    point(int v) : x(v) {}
    point(int v, std::string t) : x(v), tag(std::move(t)) {}
    point(double v, char pad = '?') : x(static_cast<int>(v)), tag(1, pad) {}
  };

  struct no_copy
  {
    int v;

    no_copy(int value) : v(value) {}
    no_copy(const no_copy&) = delete;
    no_copy(no_copy&&)      = delete;
  };

  struct aggregate
  {
    int a;
    int b;
  };
} // namespace

using namespace std::string_literals;

TEST_CASE("reflex::resolve: constructors")
{
  constexpr auto make = reflex::resolve<^^point>;

  SUBCASE("each constructor is reachable")
  {
    CHECK_EQ(make(2).x, 2);
    CHECK_EQ(make(3, "tag"s).tag, "tag");
    CHECK_EQ(make(4.5).x, 4);
    CHECK_EQ(make().x, 0);
  }
  SUBCASE("the result is the constructed type")
  {
    static_assert(std::same_as<std::invoke_result_t<decltype(make), int>, point>);
    static_assert(std::same_as<std::invoke_result_t<decltype(make)>, point>);
  }
  SUBCASE("a defaulted constructor parameter may be omitted")
  {
    CHECK_EQ(make(4.5).tag, "?");
    CHECK_EQ(make(4.5, '!').tag, "!");
    static_assert(std::invocable<decltype(make), double>);
    static_assert(std::invocable<decltype(make), double, char>);
  }
  SUBCASE("a call matching no constructor is not invocable")
  {
    static_assert(not std::invocable<decltype(make), void*>);
    static_assert(not std::invocable<decltype(make), int, int, int>);
  }
  SUBCASE("the copy constructor is part of the set")
  {
    const point p{7};
    CHECK_EQ(make(p).x, 7);
    static_assert(std::invocable<decltype(make), const point&>);
  }
  SUBCASE("a deleted constructor is not invocable")
  {
    constexpr auto make_nc = reflex::resolve<^^no_copy>;
    CHECK_EQ(make_nc(9).v, 9);
    static_assert(std::invocable<decltype(make_nc), int>);
    static_assert(not std::invocable<decltype(make_nc), const no_copy&>);
    static_assert(not std::invocable<decltype(make_nc), no_copy&&>);
  }
}

TEST_CASE("reflex::overloads_of: constructors")
{
  SUBCASE("a constructor candidate reports the constructed type")
  {
    constexpr auto first = [] consteval {
      return reflex::overloads_of(^^point, "").front();
    }();
    static_assert(first.return_type() == ^^point);
  }
  SUBCASE("every constructor arity is listed")
  {
    // 4 declared, one of them reachable at two arities, plus the implicit copy
    // and move constructors.
    constexpr auto count = reflex::overloads_of(^^point, "").size();
    static_assert(count == 7);
  }
  SUBCASE("a deleted constructor stays a candidate")
  {
    // Dropping it would silently hand the call to a worse candidate. It is
    // listed, and selecting it is what makes the call ill-formed.
    constexpr auto all = [] consteval {
      return reflex::overloads_of(^^no_copy, "");
    };
    static_assert(all().size() == 3);
    static_assert(not std::meta::is_deleted(all()[0].function));
    static_assert(std::meta::is_deleted(all()[1].function));
    static_assert(std::meta::is_deleted(all()[2].function));
  }
  SUBCASE("an aggregate exposes its implicit constructors")
  {
    // Default, copy and move. Initialization from the member list is a
    // different construction path and is covered separately.
    constexpr auto count = reflex::overloads_of(^^aggregate, "").size();
    static_assert(count == 3);
  }
}
