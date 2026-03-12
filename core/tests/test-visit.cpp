#include <doctest/doctest.h>

import reflex.core;
import std;

using namespace reflex;
using namespace std::string_literals;

auto make_variant_array(auto&&... values)
{
  using variant_type = [:substitute(
                             ^^std::variant, {
                                                 decay(^^decltype(values))...}):];
  return std::array{variant_type{std::forward<decltype(values)>(values)}...};
}

using namespace std::string_view_literals;

TEST_CASE("reflex::visit: variant array")
{
  for(auto const& item : make_variant_array(42, true, "hello"sv))
  {
    visit(
        match{
            [](int v) { CHECK_EQ(v, 42); }, [](bool v) { CHECK_EQ(v, true); },
            [](std::string_view v) { CHECK_EQ(v, "hello"); }},
        item);
  }
}

TEST_CASE("reflex::visit: non-variant")
{
  visit([](int l) { CHECK_EQ(l, 42); }, 42);
  visit(
      [](int l, int r) {
        CHECK_EQ(l, 42);
        CHECK_EQ(r, 43);
      },
      42, 43);
  visit(
      [](int a, int b, double c) {
        CHECK_EQ(a, 42);
        CHECK_EQ(b, 43);
        CHECK_EQ(c, 44.4);
      },
      42, 43, 44.4);
}

TEST_CASE("reflex::visit: variant")
{
  visit(
      match{
          [](int l) { CHECK_EQ(l, 42); },
          patterns::throw_(std::runtime_error{"unexpected visit"}),
      },
      std::variant<int, std::string>{42});

  visit(
      [](auto l, auto r) {
        CHECK_EQ(l, 42);
        CHECK_EQ(r, 43);
      },
      std::variant<int, bool>{42}, 43);

  visit(
      match{
          [](int l) { CHECK_EQ(l, 42); },
          patterns::throw_(std::runtime_error{"unexpected visit"}),
      },
      std::variant<int, std::string>{42});

  visit(
      match{
          [](std::string&& l, int r) {
            CHECK_EQ(l, "42");
            CHECK_EQ(r, 42);
          },
          patterns::throw_(std::runtime_error{"unexpected visit"}),
      },
      std::variant<int, std::string>{"42"}, std::variant<int, std::string>{42});
}

TEST_CASE("reflex::visit: variant of variant")
{
  using variant = std::variant<int, bool>;

  visit(
      match{
          [](variant&& l, variant&& r) {
            visit(
                match{
                    [](int l, int r) {
                      CHECK_EQ(l, 42);
                      CHECK_EQ(r, 43);
                    },
                    patterns::throw_(std::runtime_error{"unexpected subvariant visit"})},
                l, r);
          },
          patterns::throw_(std::runtime_error{"unexpected visit"}),
      },
      std::variant<variant, bool>{variant{42}}, std::variant<variant, bool>{variant{43}});
}