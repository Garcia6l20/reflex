#include <reflex/testing_main.hpp>

namespace base_tests
{
void test_bools()
{
  const auto a = true;
  const auto b = true;
  const auto c = false;
  check_that(a) == true;
  check_that(a);
  check_that(a) == expr(b);
  check_that(a) != expr(c);
}
[[= reflex::testing::fail_test]] void test_bools_failing()
{
  const auto a = true;
  const auto b = false;
  check_that(a) == expr(b);
}
void test_ints()
{
  const auto a = 42;
  const auto b = 66;
  const auto c = -66;
  check_that(a) == 42;
  check_that(a) != 99;
  check_that(a) >= 1;
  check_that(a) < 100;
  check_that(a) > 10;
  check_that(a) <= expr(b);
  check_that(a) > expr(c);
}
[[= reflex::testing::fail_test]] void test_ints_failing()
{
  const auto a = 42;
  const auto b = 66;
  const auto c = -66;
  check_that(a) == 66;
  check_that(a).negate() >= 1;
  check_that(a) > 100;
  check_that(a) >= expr(b);
  check_that(a) < expr(c);
}
} // namespace base_tests
namespace container_tests
{
void test_vec()
{
  check_that(std::vector<int>{}).is_empty();
  {
    auto v = std::vector{1, 2, 3};
    check_that(v).is_not_empty();
    {
      check_that(v).contains(1);
      auto value = 1;
      check_that(v).contains(expr(value));
    }
    {
      check_that(v).negate().contains(66);
      auto value = 66;
      check_that(v).negate().contains(expr(value));
    }
  }
}
[[= reflex::testing::fail_test]] void test_vec_failing()
{
  check_that(std::vector<int>{}).is_not_empty();
  {
    auto v = std::vector{1, 2, 3};
    check_that(v).is_empty();
    {
      check_that(v).negate().contains(1);
      auto value = 1;
      check_that(v).negate().contains(expr(value));
    }
    {
      check_that(v).contains(66);
      auto value = 66;
      check_that(v).contains(expr(value));
    }
  }
}
} // namespace container_tests
