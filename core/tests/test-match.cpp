#include <doctest/doctest.h>

import reflex.core;
import std;

using namespace reflex;

using namespace std::string_literals;
using namespace std::string_view_literals;

TEST_CASE("reflex::match: variant")
{
  std::variant<int, std::string>{42}
      | match{
          [](int l) { CHECK_EQ(l, 42); },
          patterns::throw_(std::runtime_error{"unexpected visit"}),
      };

  forward_as_tuple(std::variant<int, std::string>{"42"}, std::variant<int, std::string>{42})
      | match{
          [](std::string&& l, int r) {
            CHECK_EQ(l, "42");
            CHECK_EQ(r, 42);
          },
          patterns::throw_(std::runtime_error{"unexpected visit"}),
      };
}
TEST_CASE("reflex::match: variant of variant")
{
  using variant = std::variant<int, bool>;

  forward_as_tuple(
      std::variant<variant, bool>{variant{42}}, std::variant<variant, bool>{variant{43}})
      | match{
          [](variant&& l, variant&& r) {
            forward_as_tuple(l, r)
                | match{
                    [](int l, int r) {
                      CHECK_EQ(l, 42);
                      CHECK_EQ(r, 43);
                    },
                    patterns::throw_(std::runtime_error{"unexpected subvariant visit"})};
          },
          patterns::throw_(std::runtime_error{"unexpected visit"}),
      };
}