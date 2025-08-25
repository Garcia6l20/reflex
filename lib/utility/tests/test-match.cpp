#include <exception>
#include <print>

#include <reflex/match.hpp>
#include <reflex/testing_main.hpp>

using namespace reflex;
using namespace std::string_literals;

using namespace std::string_view_literals;

namespace match_tests
{
struct test_basic
{
  void test_variant()
  {
    std::variant<int, std::string>{42} | match{
                                             [](int l) { CHECK_THAT(l == 42); },
                                             patterns::throw_(std::runtime_error{"unexpected visit"}),
                                         };

    forward_as_tuple(std::variant<int, std::string>{"42"}, std::variant<int, std::string>{42}) |
        match{
            [](std::string&& l, int r)
            {
              CHECK_THAT(l == "42");
              CHECK_THAT(r == 42);
            },
            patterns::throw_(std::runtime_error{"unexpected visit"}),
        };
  }
  void test_variant_of_variant()
  {
    using variant = std::variant<int, bool>;

    forward_as_tuple(std::variant<variant, bool>{variant{42}}, std::variant<variant, bool>{variant{43}}) |
        match{
            [](variant&& l, variant&& r)
            {
              forward_as_tuple(l, r) | match{[](int l, int r)
                                             {
                                               CHECK_THAT(l == 42);
                                               CHECK_THAT(r == 43);
                                             },
                                             patterns::throw_(std::runtime_error{"unexpected subvariant visit"})};
            },
            patterns::throw_(std::runtime_error{"unexpected visit"}),
        };
  }
};
} // namespace match_tests
