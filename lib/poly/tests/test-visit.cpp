#include <reflex/var.hpp>

#include <exception>
#include <print>

using namespace reflex;

#include <reflex/testing_main.hpp>

#include <reflex/visit.hpp>

namespace var_visit_tests
{
struct test_basic
{
  using var_t = var<int, bool, std::string>;
  var_t c     = 66;

  void test1()
  {
    static_assert(is_visitable_type(^^std::variant<bool, int>));
    static_assert(is_visitable_type(^^var_t));
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
            [](int l) { CHECK_THAT(l == 66); },
            patterns::throw_(std::runtime_error{"unexpected visit"}),
        },
        c);

    visit(
        match{
            [](int l, int r)
            {
              CHECK_THAT(l == 66);
              CHECK_THAT(r == 42);
            },
            patterns::throw_(std::runtime_error{"unexpected visit"}),
        },
        c,
        std::variant<int, std::string>{42});
  }
};
} // namespace var_visit_tests
