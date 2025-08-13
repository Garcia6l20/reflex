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
