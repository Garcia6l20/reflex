#include <reflex/var.hpp>

#include <exception>
#include <print>

using namespace reflex;

#include <reflex/testing_main.hpp>

namespace var_ref_tests
{
struct test_basic
{
  using var_t = var<int, bool, std::string, int&>;
  int   value = 42;
  var_t c     = std::ref(value);

  void test_assign()
  {
    c = value;
    CHECK_THAT(c.has_value<int>());
    c = std::ref(value);
    CHECK_THAT(c.has_value<std::reference_wrapper<int>>());
    CHECK_THAT(c.has_value<int&>());
    c.get<std::reference_wrapper<int>>().get() = 43;
    CHECK_THAT(value == 43);
    ++c.get<int&>();
    CHECK_THAT(value == 44);
  }
  void test_visit()
  {
    visit(match([](int& v) { v = 43; }, patterns::throw_(std::runtime_error("unmatched reference"))), c);
    CHECK_THAT(value == 43);
  }
  void test_compare()
  {
    CHECK_THAT(c == 42);
    c += 1;
    CHECK_THAT(c == 43);
    CHECK_THAT(value == 43);
  }
  void test_copy_constructible()
  {
    var_t v = c;
    CHECK_THAT(c == 42);
    CHECK_THAT(v == 42);
    CHECK_THAT(c.has_value<int&>());
    CHECK_THAT(v.has_value<int&>());
    v += 1;
    CHECK_THAT(v == 43);
    CHECK_THAT(c == 43);
    CHECK_THAT(value == 43);
  }
};
} // namespace var_ref_tests
