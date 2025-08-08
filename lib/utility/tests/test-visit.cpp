#include <exception>
#include <print>

#include <reflex/testing_main.hpp>
#include <reflex/visit.hpp>

using namespace reflex;
using namespace std::string_literals;

namespace visit_tests
{
struct test_basic
{
  void test_non_variant_visit()
  {
    visit([](int l) { CHECK_THAT(l == 42); }, 42);
    visit(
        [](int l, int r)
        {
          CHECK_THAT(l == 42);
          CHECK_THAT(r == 43);
        },
        42,
        43);
    visit(
        [](int a, int b, double c)
        {
          CHECK_THAT(a == 42);
          CHECK_THAT(b == 43);
          CHECK_THAT(c == 44.4);
        },
        42,
        43,
        44.4);
  }
  void test_variant()
  {
    visit(
        match{
            [](int l) { CHECK_THAT(l == 42); },
            patterns::throw_(std::runtime_error{"unexpected visit"}),
        },
        std::variant<int, std::string>{42});

    visit(
        [](auto l, auto r)
        {
          CHECK_THAT(l == 42);
          CHECK_THAT(r == 43);
        },
        std::variant<int, bool>{42},
        43);

    visit(
        match{
            [](int l) { CHECK_THAT(l == 42); },
            patterns::throw_(std::runtime_error{"unexpected visit"}),
        },
        std::variant<int, std::string>{42});

    visit(
        match{
            [](std::string&& l, int r)
            {
              CHECK_THAT(l == "42");
              CHECK_THAT(r == 42);
            },
            patterns::throw_(std::runtime_error{"unexpected visit"}),
        },
        std::variant<int, std::string>{"42"},
        std::variant<int, std::string>{42});
  }
  void test_variant_of_variant()
  {
    using variant = std::variant<int, bool>;

    visit(
        match{
            [](variant&& l, variant&& r)
            {
              visit(match{[](int l, int r)
                          {
                            CHECK_THAT(l == 42);
                            CHECK_THAT(r == 43);
                          },
                          patterns::throw_(std::runtime_error{"unexpected subvariant visit"})},
                    l,
                    r);
            },
            patterns::throw_(std::runtime_error{"unexpected visit"}),
        },
        std::variant<variant, bool>{variant{42}},
        std::variant<variant, bool>{variant{43}});
  }
};
} // namespace visit_tests
