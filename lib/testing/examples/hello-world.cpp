#include <reflex/testing_main.hpp>

// a test suite is a namespace containing "test"
namespace hello_world_tests
{
  
// a simple function test is a function containing "test"
void test_hello()
{
  using namespace std::string_view_literals;
  CHECK_THAT("hello"sv == "world"sv);
}

// a sub-test-suite is can be a struct containing "test"
struct hello_test_suite
{
  std::string hello = "hello";

  void test1()
  {
    CHECK_THAT(hello == "world"); // hello will be expanded here since its known in current evaluation context
  }
};

// a sub-test-suite is can be a namespace containing "test"
namespace sub_tests
{
void test_hello()
{
  using namespace std::string_view_literals;
  CHECK_THAT("hello"sv == "world"sv);
}
} // namespace sub_tests

} // namespace hello_world_tests
