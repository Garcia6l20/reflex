#pragma once

#include <reflex/pp.hpp>
#include <reflex/testing/validator.hpp>

#include <functional>
#include <map>
#include <meta>
#include <print>
#include <vector>

namespace reflex::testing
{
namespace detail
{
template <meta::access_context Ctx, typename Expr>
constexpr auto _ensure(Expr&& expression, bool fatal, std::source_location const& l = std::source_location::current())
{
  return detail::validator<Expr, Ctx>{std::forward<Expr>(expression), fatal, l};
}

template <auto result, meta::access_context Ctx>
constexpr auto _static_ensure(std::string_view            expression_str,
                              bool                        fatal,
                              std::source_location const& l = std::source_location::current())
{
  static constexpr auto fn = [] { return result; };
  using evaluator_type     = reflex::testing::detail::evaluator<decltype(fn)>;
  return detail::validator<evaluator_type, Ctx>{evaluator_type(fn, expression_str), fatal, l};
}
} // namespace detail

constexpr detail::__fail_test fail_test;

#define __MAKE_LAZY_ASSERTER(__fatal__, ...)                                                             \
  reflex::testing::detail::_ensure<std::meta::access_context::current()>(                                \
      reflex::testing::detail::evaluator([&] -> decltype(auto) { return (__VA_ARGS__); }, #__VA_ARGS__), \
      __fatal__)

#define ASSERT_THAT_LAZY(...) __MAKE_LAZY_ASSERTER(true, __VA_ARGS__)
#define CHECK_THAT_LAZY(...)  __MAKE_LAZY_ASSERTER(false, __VA_ARGS__)

#define __MAKE_ASSERTER(__fatal__, ...)                                   \
  reflex::testing::detail::_ensure<std::meta::access_context::current()>( \
      reflex::testing::expression((__VA_ARGS__), #__VA_ARGS__),           \
      __fatal__)

#define ASSERT_THAT(...) __MAKE_ASSERTER(true, __VA_ARGS__)
#define CHECK_THAT(...)  __MAKE_ASSERTER(false, __VA_ARGS__)

#define __REFLEXPR(item)             ^^item
#define __MAKE_VARIADIC_RELEXPR(...) PP_COMMA_JOIN(__REFLEXPR, __VA_ARGS__)

#define CHECK_CONTEXT(...)                                                                       \
  const auto PP_CONCAT(__ctx_, __LINE__) = reflex::testing::detail::eval_context<__MAKE_VARIADIC_RELEXPR(__VA_ARGS__)> \
  {                                                                                              \
    __VA_ARGS__                                                                                  \
  }

#define STATIC_CHECK_THAT(...) \
  reflex::testing::detail::_static_ensure<(__VA_ARGS__), std::meta::access_context::current()>(#__VA_ARGS__, false)

template <std::meta::info... NSs> int run_all(int argc, char** argv)
{
  using namespace std::meta;
  constexpr auto suites = []
  {
    if constexpr(sizeof...(NSs) == 0)
    {
      return define_static_array(
          members_of(^^::, access_context::unchecked()) |
          std::views::filter([](auto R) { return is_namespace(R) and identifier_of(R).contains("test"); }) |
          std::ranges::to<std::vector>());
    }
    else
    {
      return std::array{NSs...};
    }
  }();
  template for(constexpr auto suite : suites)
  {
    template for(constexpr auto R : define_static_array(members_of(suite, access_context::unchecked())))
    {
      if constexpr(is_function(R))
      {
        constexpr auto id = identifier_of(R);
        if constexpr(id.contains("test"))
        {
          std::println("Running: {}...", id);
          [:R:]();
        }
      }
    }
  }
  return detail::get_reporter().finish();
}
} // namespace reflex::testing
