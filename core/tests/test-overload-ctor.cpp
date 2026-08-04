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

  struct config
  {
    int         port;
    std::string host;
  };

  struct with_defaults
  {
    int a = 1;
    int b = 2;
  };

  struct nested
  {
    config c;
    int    n;
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

TEST_CASE("reflex::resolve: aggregate initialization")
{
  constexpr auto make = reflex::resolve<^^config>;

  SUBCASE("the member list is a construction path of its own")
  {
    const auto c = make(8080, "localhost"s);
    CHECK_EQ(c.port, 8080);
    CHECK_EQ(c.host, "localhost");
  }
  SUBCASE("trailing members may be omitted")
  {
    CHECK_EQ(make(8080).host, "");
    CHECK_EQ(make().port, 0);
    static_assert(std::invocable<decltype(make)>);
    static_assert(std::invocable<decltype(make), int>);
    static_assert(std::invocable<decltype(make), int, std::string>);
  }
  SUBCASE("a member cannot be skipped, only dropped from the end")
  {
    static_assert(not std::invocable<decltype(make), std::string>);
    static_assert(not std::invocable<decltype(make), int, std::string, int>);
  }
  SUBCASE("a default member initializer is used for an omitted member")
  {
    constexpr auto wd = reflex::resolve<^^with_defaults>;
    CHECK_EQ(wd().a, 1);
    CHECK_EQ(wd().b, 2);
    CHECK_EQ(wd(9).a, 9);
    CHECK_EQ(wd(9).b, 2);
  }
  SUBCASE("an aggregate member is initialized like any other")
  {
    constexpr auto mk = reflex::resolve<^^nested>;
    const auto     n  = mk(config{80, "h"}, 5);
    CHECK_EQ(n.c.port, 80);
    CHECK_EQ(n.n, 5);
  }
  SUBCASE("the copy constructor still wins over a one member initialization")
  {
    const config src{1, "a"};
    const auto   copy = make(src);
    CHECK_EQ(copy.host, "a");
    static_assert(std::same_as<std::invoke_result_t<decltype(make), const config&>, config>);
  }
  SUBCASE("a type with declared constructors keeps only those")
  {
    // point is not an aggregate, so its member list is not a construction path.
    constexpr auto mk = reflex::resolve<^^point>;
    static_assert(not std::invocable<decltype(mk), int, std::string, int>);
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
  SUBCASE("an aggregate lists its member list next to its implicit constructors")
  {
    // Copy and move, plus one member list prefix per arity. The implicit
    // default constructor is dropped: the empty prefix covers the same call.
    constexpr auto count = reflex::overloads_of(^^aggregate, "").size();
    static_assert(count == 5);
  }
  SUBCASE("an aggregate initialization candidate names the type, not a function")
  {
    constexpr auto last = [] consteval {
      return reflex::overloads_of(^^config, "").back();
    }();
    static_assert(last.is_aggregate_init());
    static_assert(last.function == ^^config);
    static_assert(last.return_type() == ^^config);
    static_assert(last.arity == 2);
    static_assert(last.parameter_types().front() == ^^int);
    static_assert(std::meta::dealias(last.parameter_types().back())
                  == std::meta::dealias(^^std::string));
  }
}
