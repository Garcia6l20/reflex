#include <reflex/testing_main.hpp>

namespace parametrize_tests
{
using namespace reflex::testing;

struct fixture_data_item
{
  int a;
  int b;
  int expected;
};

static const auto fixture_data = std::array{//
                                            fixture_data_item{1, 1, 0},
                                            fixture_data_item{2, 1, 1},
                                            fixture_data_item{1, 2, -1},
                                            fixture_data_item{45, 3, 42}};

[[= parametrize<^^fixture_data>]] void test_diff(int a, int b, int expected)
{
  check_that(a - b) == expr(expected);
}

static const auto fixture_data2 = std::array{//
                                            std::tuple{1, 1, 0},
                                            std::tuple{2, 1, 1},
                                            std::tuple{1, 2, -1},
                                            std::tuple{45, 3, 42}};

[[= parametrize<^^fixture_data2>]] void test_diff2(int a, int b, int expected)
{
  check_that(a - b) == expr(expected);
}
} // namespace parametrize_tests