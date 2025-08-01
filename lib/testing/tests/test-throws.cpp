#include <reflex/testing_main.hpp>

namespace throwing_tests
{
void test()
{
  check_that(42) == 42;
  check_that(throw std::runtime_error{"yeah"}).throws<std::runtime_error>();
  check_that(throw std::runtime_error{"yeah"}).negate().throws<std::bad_alloc>();
}
} // namespace fixure_tests