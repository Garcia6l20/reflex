#include <reflex/testing_main.hpp>

namespace fixure_tests
{
struct with_simple_fixture_tests
{
  std::vector<int> data = {2, 4, 6, 7};

  void test1()
  {
    check_that(data).negate().is_empty();
  }
  void test2()
  {
    check_that(data).contains(2);
  }
};
} // namespace fixure_tests