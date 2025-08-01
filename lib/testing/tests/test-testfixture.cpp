#include <reflex/testing_main.hpp>

namespace fixure_tests
{
struct with_simple_fixture_tests
{
  std::vector<int> data = {2, 4, 6, 7};

  void test1()
  {
    check_that(data).negate().is_empty();
    using std::ranges::count_if;
    auto is_odd = [](auto const& i) { return i % 2 == 0; };
    bool test   = true;
    check_that(count_if(data, is_odd)).with_extra(expr(data), expr(test)) == 3; // explicit extra info
    // check_that(count_if(data, is_odd)) == 1;
    // expanded info will show '👉  3 == 1 (with: data=[2, 4, 6, 7])'
    // as data can be found in test scope (aka.: with_simple_fixture_tests class)
  }
  [[= reflex::testing::fail_test]] void test2()
  {
    check_that(data).negate().contains(2);
  }
};
} // namespace fixure_tests