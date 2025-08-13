#include <reflex/args.hpp>
#include <reflex/constant.hpp>

#include <reflex/testing_main.hpp>

namespace reflex
{

} // namespace reflex

using namespace reflex;

namespace args_tests
{
// namespace kw_args_tests
// {
// void test1()
// {
//   auto kwargs = reflex_kwargs(a = 42, b = 43);
//   // constexpr auto ctx    = meta::access_context::unchecked();
//   // constexpr auto I      = type_of(bases_of(type_of(^^kwargs), ctx)[0]);
//   // template for(constexpr auto M : define_static_array(nonstatic_data_members_of(I, ctx)))
//   // {
//   //   std::println("{}", identifier_of(M));
//   // }
//   CHECK_THAT(kwargs.a == 42);
//   CHECK_THAT(kwargs.b == 43);
// }
// void test2()
// {
//   [](auto const& kwargs)
//   {
//     CHECK_THAT(kwargs.a == 42);
//     CHECK_THAT(kwargs.b == 43.3);
//   }(reflex_kwargs(a = 42, b = 43.3));
// }
// } // namespace kw_args_tests

// namespace extract_tests
// {
// void test_extract()
// {
//   [](auto&&... args)
//   {
//     auto&& [... ints] = extract_args_if<[](auto type) { return type == ^^int&&; }>(reflex_fwd(args)...);
//     STATIC_CHECK_THAT(sizeof...(ints) == 2);
//     CHECK_THAT(ints...[0] == 42);
//     CHECK_THAT(ints...[1] == 43);
//   }(42, 43);
//   [](auto&&... args)
//   {
//     auto&& [... ints] = extract_args_if<[](auto type) { return type == ^^int&&; }>(reflex_fwd(args)...);
//     STATIC_CHECK_THAT(sizeof...(ints) == 2);
//     CHECK_THAT(ints...[0] == 42);
//     CHECK_THAT(ints...[1] == 43);

//     auto&& [... doubles] = extract_args_if<[](auto type) { return type == ^^double&&; }>(reflex_fwd(args)...);
//     STATIC_CHECK_THAT(sizeof...(doubles) == 2);
//     CHECK_THAT(doubles...[0] == 42.2);
//     CHECK_THAT(doubles...[1] == 43.3);
//   }(42, 42.2, 43, 43.3);
// }
// void test_extract_by_value_category()
// {
//   int i1 = 42;
//   [](auto&&... args)
//   {
//     auto&& [... lv_ints] = extract_args_if<[](auto T) { return is_lvalue_reference_type(T); }>(reflex_fwd(args)...);
//     STATIC_CHECK_THAT(sizeof...(lv_ints) == 1);
//     CHECK_THAT(lv_ints...[0] == 42);
//     auto&& lval = reflex_fwd(lv_ints...[0]);
//     STATIC_CHECK_THAT(is_lvalue_reference_type(type_of(^^lval)));

//     auto&& [... rv_ints] = extract_args_if<[](auto T) { return is_rvalue_reference_type(T); }>(reflex_fwd(args)...);
//     STATIC_CHECK_THAT(sizeof...(rv_ints) == 1);
//     CHECK_THAT(rv_ints...[0] == 43);
//     auto&& rval = reflex_fwd(rv_ints...[0]);
//     CHECK_THAT(rval == 43);
//     STATIC_CHECK_THAT(is_rvalue_reference_type(type_of(^^rval)));
//   }(i1, 43);
// }
// } // namespace extract_tests

using namespace reflex::literals;

namespace pack_tests
{
void test_value_category_preservation()
{
  [](auto&&... args)
  {
    auto p = reflex_pack(args);
    static_assert(is_rvalue_reference_type(^^decltype(p[0_uz])));
    static_assert(is_rvalue_reference_type(^^decltype(p[1_uz])));

    auto&& [... ints] = get_if<[](auto type) { return type == ^^int&&; }>(p);
    STATIC_CHECK_THAT(sizeof...(ints) == 2);
    CHECK_THAT(ints...[0] == 42);
    CHECK_THAT(ints...[1] == 43);
  }(42, 43);
}
void test_select_base()
{
  int    a = 55, b = 44;
  double c = 55.5, d = 44.4;
  auto   p = forward_as_pack(a, c, b, d);
  auto [ints, doubles, remaining] =
      p.select<[](auto t) { return t == ^^int&; }, [](auto t) { return t == ^^double&; }>();
  static_assert(ints.size == 2);
  static_assert(doubles.size == 2);
  static_assert(remaining.size == 0);
  CHECK_THAT(ints[0_uz] == 55);
  CHECK_THAT(ints[1_uz] == 44);
  CHECK_THAT(doubles[0_uz] == 55.5);
  CHECK_THAT(doubles[1_uz] == 44.4);
}
void test_select_preservation()
{
  [](auto&&... args)
  {
    auto [int_p, remaining_p] = select<[](auto type) { return type == ^^int&&; }>(reflex_pack(args));
    static_assert(is_rvalue_reference_type(^^decltype(int_p[0_uz])));
    static_assert(is_rvalue_reference_type(^^decltype(int_p[1_uz])));
    
    auto&& [... ints]         = int_p;
    STATIC_CHECK_THAT(sizeof...(ints) == 2);
    CHECK_THAT(ints...[0] == 42);
    CHECK_THAT(ints...[1] == 43);
  }(42, 43);
}
} // namespace pack_tests

} // namespace args_tests
