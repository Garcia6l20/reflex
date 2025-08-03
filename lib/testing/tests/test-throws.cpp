#include <reflex/testing_main.hpp>

namespace throwing_tests
{
void test()
{
  CHECK_THAT(42) == 42;
  CHECK_THAT_LAZY(throw std::runtime_error{"yeah"}).throws<std::runtime_error>();
  CHECK_THAT_LAZY(throw std::runtime_error{"yeah"}).negate().throws<std::bad_alloc>();
}
} // namespace throwing_tests