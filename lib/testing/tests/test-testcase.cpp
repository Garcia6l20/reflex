#include <reflex/testing_main.hpp>

namespace base_tests
{
void test_bools()
{
  const auto a = true;
  const auto b = true;
  const auto c = false;
  check_that(rr(a)) == true;
  check_that(rr(a));
  check_that(rr(a)) == false;
  check_that(rr(a)) == rr(b);
  check_that(rr(a)) == rr(c);
}
void test_ints()
{
  const auto a = 42;
  const auto b = 66;
  const auto c = -66;
  check_that(rr(a)) == 66;
  check_that(rr(a)) != 99;
  check_that(rr(a)) >= 1;
  check_that(rr(a)) < 100;
  check_that(rr(a)) > 100;
  check_that(rr(a)) >= rr(b);
  check_that(rr(a)) < rr(c);
}
} // namespace base_tests
namespace container_tests
{
void test_vec()
{
  check_that(std::vector<int>{}).is_empty();
  check_that(std::vector<int>{}).is_not_empty();
  {
    auto v = std::vector{1, 2, 3};
    check_that(rr(v)).is_empty();
    check_that(rr(v)).is_not_empty();
    {
      check_that(rr(v)).contains(1);
      auto value = 1;
      check_that(rr(v)).contains(rr(value));
    }
    {
      check_that(rr(v)).contains(66);
      auto value = 66;
      check_that(rr(v)).contains(rr(value));
    }

    // comming soon
    // check_that(rr(v)).view() | std::views::filter([](const auto& i) { return i % 2 == 0; }) | is_length(1);
  }
}
} // namespace container_tests
