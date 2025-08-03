#include <reflex/testing_main.hpp>

namespace parametrize_tests
{
using namespace reflex::testing;

void test_simple_context()
{
  int a = 42;
  int b = 43;
  CHECK_CONTEXT(a, b);
  CHECK_THAT(a != b);
  CHECK_THAT(a < b);

  int ab = a * b;
  int aba = a * b * a;
  CHECK_CONTEXT(ab, aba);
  CHECK_THAT(ab == a * b);
  CHECK_THAT(aba == a * b * a);
}

} // namespace parametrize_tests