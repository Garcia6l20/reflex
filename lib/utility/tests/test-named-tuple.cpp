#include <reflex/named_tuple.hpp>

#include <reflex/testing_main.hpp>

using namespace reflex;

namespace named_tuple_tests
{

void test_make_named_tuple_explicit()
{
  constexpr auto t = make_named_tuple<"hello", "world">(1, 2);
  CHECK_THAT(t.hello) == 1;
  CHECK_THAT(t.get<"hello">()) == 1;
  CHECK_THAT(get<"hello">(t)) == 1;
  CHECK_THAT(t.world) == 2;
  CHECK_THAT(t.get<"world">()) == 2;
  CHECK_THAT(get<"world">(t)) == 2;

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
  CHECK_THAT(t.f) == 42.2f;
  CHECK_THAT(t.get<"f">()) == 42.2f;
  CHECK_THAT(get<"f">(t)) == 42.2f;
  CHECK_THAT(t.i) == 42;
  CHECK_THAT(t.get<"i">()) == 42;
  CHECK_THAT(get<"i">(t)) == 42;
}

void test_non_constexpr_to_tuple()
{
  struct test_s
  {
    float f;
    int   i;
  };
  auto t = make_named_tuple(test_s{42.2f, 42});
  CHECK_THAT(t.f) == 42.2f;
  CHECK_THAT(t.get<"f">()) == 42.2f;
  CHECK_THAT(get<"f">(t)) == 42.2f;
  CHECK_THAT(t.i) == 42;
  CHECK_THAT(t.get<"i">()) == 42;
  CHECK_THAT(get<"i">(t)) == 42;

  auto tt = to_tuple(t);
}
} // namespace named_tuple_tests