#include <reflex/var.hpp>

#include <exception>
#include <print>

using namespace reflex;

#include <reflex/testing_main.hpp>

namespace var_tests
{
struct test_basic
{
  using var_t = var<int, bool, std::string>;
  var_t a, b;

  void test_add()
  {
    a = 42;
    b = 43;
    CHECK_THAT(a + b == 85);
    CHECK_THAT(a + 3 == 45);
    auto r = 3 + a;
    static_assert(type_of(^^r) == ^^int);
    CHECK_THAT(r == 45);
    
  }
  void test_sub()
  {
    a = 42;
    b = 43;
    CHECK_THAT(a - b == -1);
    CHECK_THAT(a - 2 == 40);
    auto r = 45 - a;
    static_assert(type_of(^^r) == ^^int);
    CHECK_THAT(r == 3);
  }
  void test_mult()
  {
    a = 42;
    b = 2;
    CHECK_THAT(a * b == 84);
    CHECK_THAT(a * 2 == 84);
    auto r = 2 * a;
    static_assert(type_of(^^r) == ^^int);
    CHECK_THAT(r == 84);
  }
  void test_div()
  {
    a = 42;
    b = 2;
    CHECK_THAT(a / b == 21);
    CHECK_THAT(a / 2 == 21);

    auto r = 84 / a;
    static_assert(type_of(^^r) == ^^int);
    CHECK_THAT(r == 2);
  }
  void test_mod()
  {
    a = 44;
    b = 3;
    CHECK_THAT(a % b == 2);
    a = 44;
    CHECK_THAT(a % 5 == 4); 
  }
  void test_self_add()
  {
    a = 2;
    b = 3;
    a += b;
    CHECK_THAT(a == 5);
    a += 2;
    CHECK_THAT(a == 7);
  }
  void test_self_sub()
  {
    a = 7;
    b = 3;
    a -= b;
    CHECK_THAT(a == 4);
    a -= 2;
    CHECK_THAT(a == 2);
  }
  void test_self_mult()
  {
    a = 3;
    b = 2;
    a *= b;
    CHECK_THAT(a == 6);
    a *= 5;
    CHECK_THAT(a == 30);
  }
  void test_self_div()
  {
    a = 60;
    b = 2;
    a /= b;
    CHECK_THAT(a == 30);
    a /= 2;
    CHECK_THAT(a == 15);
  }
  void test_self_mod()
  {
    a = 60;
    b = 31;
    a %= b;
    CHECK_THAT(a == 29);
    a %= 16;
    CHECK_THAT(a == 13);
  }
};

} // namespace var_tests
