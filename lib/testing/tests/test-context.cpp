#include <reflex/testing_main.hpp>

namespace parametrize_tests
{
using namespace reflex::testing;

void test_simple_context()
{
  int a = 42;
  int b = 43;
  check_context(a, b);
  check_that(a == b);
  check_that(a > b);
  check_that(a < b);
}

} // namespace parametrize_tests