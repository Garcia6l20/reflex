#include <reflex/testing_main.hpp>

namespace base_tests
{
void test_bools()
{
  const auto a = true;
  const auto b = true;
  const auto c = false;
  CHECK_THAT(a) == true;
  CHECK_THAT(a);
  CHECK_THAT(a) == expr(b);
  CHECK_THAT(a) != expr(c);
}
[[= reflex::testing::fail_test]] void test_bools_failing()
{
  const auto a = true;
  const auto b = false;
  CHECK_THAT(a) == expr(b);
}
void test_ints()
{
  const auto a = 42;
  const auto b = 66;
  const auto c = -66;
  CHECK_THAT(a) == 42;
  CHECK_THAT(a) != 99;
  CHECK_THAT(a) >= 1;
  CHECK_THAT(a) < 100;
  CHECK_THAT(a) > 10;
  CHECK_THAT(a) <= expr(b);
  CHECK_THAT(a) > expr(c);
}
[[= reflex::testing::fail_test]] void test_ints_failing()
{
  const auto a = 42;
  const auto b = 66;
  const auto c = -66;
  CHECK_THAT(a) == 66;
  CHECK_THAT(a).negate() >= 1;
  CHECK_THAT(a) > 100;
  CHECK_THAT(a) >= expr(b);
  CHECK_THAT(a) < expr(c);
}
} // namespace base_tests
namespace container_tests
{
void test_vec()
{
  CHECK_THAT(std::vector<int>{}).is_empty();
  {
    auto v = std::vector{1, 2, 3};
    CHECK_THAT(v).is_not_empty();
    {
      CHECK_THAT(v).contains(1);
      auto value = 1;
      CHECK_THAT(v).contains(expr(value));
    }
    {
      CHECK_THAT(v).negate().contains(66);
      auto value = 66;
      CHECK_THAT(v).negate().contains(expr(value));
    }
  }
}
[[= reflex::testing::fail_test]] void test_vec_failing()
{
  CHECK_THAT(std::vector<int>{}).is_not_empty();
  {
    auto v = std::vector{1, 2, 3};
    CHECK_THAT(v).is_empty();
    {
      CHECK_THAT(v).negate().contains(1);
      auto value = 1;
      CHECK_THAT(v).negate().contains(expr(value));
    }
    {
      CHECK_THAT(v).contains(66);
      auto value = 66;
      CHECK_THAT(v).contains(expr(value));
    }
  }
}
} // namespace container_tests
