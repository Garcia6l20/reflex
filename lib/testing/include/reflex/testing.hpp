#pragma once

#include <reflex/testing/reflected_ref.hpp>

#include <map>
#include <meta>
#include <print>
#include <vector>

namespace reflex::testing
{
namespace detail
{
struct validation_result
{
  bool                 result;
  std::string          expression;
  std::string          expanded_expression;
  std::source_location location;
  operator bool() const
  {
    return result;
  }
};

struct reporter_t
{
  template <auto ctx> void append(validation_result&& result)
  {
    constexpr auto scope      = ctx.scope();
    auto           test_suite = std::string{display_string_of(parent_of(scope))};
    auto           test_case  = std::string{display_string_of(scope)};
    if(not results.contains(test_suite))
    {
      results[test_suite] = std::map<std::string, std::vector<validation_result>>{};
    }
    if(not results[test_suite].contains(test_case))
    {
      results[test_suite][test_case] = std::vector<validation_result>{};
    }
    results[test_suite][test_case].push_back(std::move(result));
  }

  int finish() const
  {
    using std::ranges::any_of;
    using std::views::filter;
    auto failed_suites = results //
                         | filter(
                               [](auto const& test_cases)
                               {
                                 return any_of(test_cases.second, [](auto const& test_case) {//
                                   return any_of(test_case.second, [](auto const& result) { return not result; });
                                 });
                               });

    size_t total_failed_case_count = 0;

    if(not failed_suites.empty())
    {
      for(const auto& [suite, test_cases] : failed_suites)
      {
        std::println("❌ suite '{}' failed:", suite);

        auto failed_cases = test_cases //
                            |
                            filter([](auto const& test_case)
                                   { return any_of(test_case.second, [](auto const& result) { return not result; }); });
        for(const auto& [test_case, res_vec] : failed_cases)
        {
          std::println("  ❌ case '{}' failed:", test_case);

          auto failed_results = res_vec //
                                | filter([](auto const& result) { return not result; });
          for(const validation_result& result : failed_results)
          {
            std::println("    ❌ check failed: {}", result.expression);
            if(result.expression != result.expanded_expression)
            {
              std::println("                  👉 {}", result.expanded_expression);
            }
            std::println("       {}:{}", result.location.file_name(), result.location.line());
          }
          ++total_failed_case_count;
        }
      }
    }
    return total_failed_case_count;
  }

  std::map<std::string, std::map<std::string, std::vector<validation_result>>> results;
};
inline static reporter_t reporter;

template <typename Expr, meta::access_context Ctx> struct validator
{
  Expr const&                 e_;
  std::source_location const& l_;
  mutable bool                wip_   = true;
  const bool                  fatal_ = true;

  validator(Expr const& e, bool fatal, std::source_location const& l) : e_{e}, l_{l}, fatal_{fatal}
  {
  }

  ~validator() noexcept(false)
  {
    if(wip_)
    {
      if constexpr(requires() { *this == true; })
      {
        *this == true;
      }
      else
      {
        std::println("nothing asserted !");
        std::abort();
      }
    }
  }

  auto _loc_str() const
  {
    return std::format("{}:{}", l_.file_name(), l_.line());
  }

  decltype(auto) expression() const
  {
    if constexpr(is_reflected_ref(^^Expr))
    {
      return e_.get();
    }
    else
    {
      return e_;
    }
  }
  constexpr decltype(auto) expression_string() const
  {
    if constexpr(is_reflected_ref(^^Expr))
    {
      return display_string_of(std::decay_t<Expr>::info);
    }
    else
    {
      return e_;
    }
  }

  template <typename Fn> bool validate(Fn&& fn) const
  {
    wip_                     = false;
    validation_result result = fn(*this);
    result.location          = l_;
    if(not result)
    {
      std::println("failed: {} [{}] ({})", result.expression, result.expanded_expression, _loc_str());
      if(fatal_)
      {
        std::abort();
      }
    }
    reporter.template append<Ctx>(std::move(result));
    return true;
  }

  template <typename T> static constexpr auto expected_expression(T&& expected)
  {
    if constexpr(is_reflected_ref(^^T))
    {
      return display_string_of(std::decay_t<T>::info);
    }
    else
    {
      return expected;
    }
  }

#define _X_TESTING_OPERATIONS(X)   \
  X(==, _is_equal_to)              \
  X(!=, _is_not_equal_to)          \
  X(>, _is_greater_than)           \
  X(>=, _is_greater_or_equal_than) \
  X(<, _is_less_than)              \
  X(<=, _is_less_or_equal_than)

#define X(__op, __name)                                                           \
  template <typename T> constexpr void __name(T&& expected) const                 \
  {                                                                               \
    const auto expected_ex = [&] { return expected_expression(expected); };       \
    validate(                                                                     \
        [&](auto&)                                                                \
        {                                                                         \
          return detail::validation_result{                                       \
              expression() __op expected,                                         \
              std::format("{} {} {}", expression_string(), #__op, expected_ex()), \
              std::format("{} {} {}", expression(), #__op, expected),             \
          };                                                                      \
        });                                                                       \
  }                                                                               \
  template <typename T>                                                           \
  constexpr void operator __op(T&& expected) const                                \
    requires requires() { expression() __op expected; }                           \
  {                                                                               \
    __name(std::forward<T>(expected));                                            \
  }
  _X_TESTING_OPERATIONS(X)
#undef X

  constexpr decltype(auto) is_empty() const
  {
    validate(
        [&](auto&)
        {
          return detail::validation_result{
              expression().empty(),
              std::format("{} is empty", expression_string()),
              std::format("{} is empty", expression()),
          };
        });
    return *this;
  }
  constexpr decltype(auto) is_not_empty() const
  {
    validate(
        [&](auto&)
        {
          return detail::validation_result{
              not expression().empty(),
              std::format("{} is not empty", expression_string()),
              std::format("{} is not empty", expression()),
          };
        });
    return *this;
  }
  template <typename T> constexpr decltype(auto) contains(T&& expected) const
  {
    const auto expected_ex = [&] { return expected_expression(expected); };
    validate(
        [&](auto&)
        {
          return detail::validation_result{
              std::ranges::contains(expression(), expected),
              std::format("{} contains {}", expression_string(), expected_ex()),
              std::format("{} contains {}", expression(), expected),
          };
        });
    return *this;
  }
  constexpr decltype(auto) view() const
    requires std::ranges::range<decltype(expression())>
  {
    return expression();
  }
};
} // namespace detail
template <meta::access_context Ctx, typename Expr>
auto _ensure(Expr const& expression, bool fatal, std::source_location const& l = std::source_location::current())
{
  return detail::validator<Expr, Ctx>{expression, fatal, l};
}

#define assert_that(_expression) reflex::testing::_ensure<std::meta::access_context::current()>((_expression), true)
#define check_that(_expression)  reflex::testing::_ensure<std::meta::access_context::current()>((_expression), false)

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
  return detail::reporter.finish();
}
} // namespace reflex::testing
