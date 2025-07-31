#include <reflex/named_tuple.hpp>

#include <reflex/testing_main.hpp>

using namespace reflex;

namespace named_tuple_tests
{

void test_make_named_tuple_explicit()
{
  constexpr auto t = make_named_tuple<"hello", "world">(1, 2);
  check_that(t.hello) == 1;
  check_that(t.get<"hello">()) == 1;
  check_that(get<"hello">(t)) == 1;
  check_that(t.world) == 2;
  check_that(t.get<"world">()) == 2;
  check_that(get<"world">(t)) == 2;

  std::println("============= all members =============");
  constexpr auto T = type_of(^^t);
  template for(constexpr auto I : define_static_array(meta::members_of_r(T, meta::access_context::current())))
  {
    println("{}", display_string_of(I));
  }

  std::println("============= nsd members =============");
  template for(constexpr auto I :
               define_static_array(meta::nonstatic_data_members_of_r(T, meta::access_context::current())))
  {
    println("{}", display_string_of(I));
  }

  constexpr auto tt = to_tuple(t);
  std::println("============= std::tuple values =============");
  template for(constexpr auto v : tt)
  {
    std::println("{}", v);
  }
}

void test_make_named_tuple_from_struct()
{
  struct test_s
  {
    float f;
    int   i;
  };
  constexpr auto t = make_named_tuple(test_s{42.2f, 42});
  check_that(t.f) == 42.2f;
  check_that(t.get<"f">()) == 42.2f;
  check_that(get<"f">(t)) == 42.2f;
  check_that(t.i) == 42;
  check_that(t.get<"i">()) == 42;
  check_that(get<"i">(t)) == 42;
}

void test_non_constexpr_to_tuple()
{
  struct test_s
  {
    float f;
    int   i;
  };
  auto t = make_named_tuple(test_s{42.2f, 42});
  check_that(t.f) == 42.2f;
  check_that(t.get<"f">()) == 42.2f;
  check_that(get<"f">(t)) == 42.2f;
  check_that(t.i) == 42;
  check_that(t.get<"i">()) == 42;
  check_that(get<"i">(t)) == 42;

  auto tt = to_tuple(t);
}
} // namespace named_tuple_tests