#pragma once

#include <reflex/testing/context.hpp>
#include <reflex/testing/evaluator.hpp>
#include <reflex/testing/expression.hpp>
#include <reflex/testing/reporter.hpp>

#include <reflex/pp.hpp>

#include <functional>
#include <map>
#include <meta>
#include <print>
#include <vector>

namespace reflex::testing
{
namespace detail
{

template <typename Evaluator, meta::access_context Ctx> struct validator
{
  Evaluator eval_;
  using value_type = typename Evaluator::value_type;

  // using expression_type = Expr;
  std::source_location                      l_;
  bool                                      wip_     = true;
  const bool                                fatal_   = true;
  bool                                      negated_ = false;
  std::vector<std::function<std::string()>> extra_descs_;
  static constexpr auto                     scope_         = Ctx.scope();
  static constexpr auto                     parent_scope_  = parent_of(scope_);
  static constexpr auto                     is_test_class_ = is_type(parent_scope_) and is_class_type(parent_scope_);
  static constexpr auto                     is_fail_test_  = is_fail_test(scope_);
  static constexpr auto                     has_value_     = Evaluator::has_value;

  constexpr validator(Evaluator&& e, bool fatal, std::source_location const& l) : eval_{e}, l_{l}, fatal_{fatal}
  {
  }

  ~validator() noexcept(false)
  {
    if(wip_)
    {
      if constexpr(has_value_ and requires() { *this == true; })
      {
        validate(
            [this](auto&)
            {
              return detail::validation_result{
                  value() == true,
                  std::format("{} evaluates to false", expression_string()),
              };
            });
      }
      else if constexpr(not has_value_)
      {
        // function expression validity check - just fetch value
        value();
      }
      else
      {
        std::println("nothing asserted !");
        std::abort();
      }
    }
  }

  template <typename... Expression>
    requires(is_expression(^^Expression) and ...)
  constexpr decltype(auto) with_extra(Expression&&... extra)
  {
    template for(constexpr auto ii : std::views::iota(0uz, sizeof...(Expression)))
    {
      extra_descs_.push_back([e = std::forward<decltype(extra...[ii])>(extra...[ii])]
                             { return std::format("{}={}", e.str(), e); });
    }
    return *this;
  }

  constexpr decltype(auto) negate()
  {
    negated_ = !negated_;
    return *this;
  }

  auto _loc_str() const
  {
    return std::format("{}:{}", l_.file_name(), l_.line());
  }

  constexpr decltype(auto) value() const
  {
    return eval_.get();
  }

  constexpr decltype(auto) expression_string() const
  {
    return eval_.str();
  }

  template <typename Fn> constexpr bool validate(Fn&& fn)
  {
    wip_                     = false;
    validation_result result = fn(*this);
    result.location          = l_;
    result.result            = result != is_fail_test_;
    result.result            = result != negated_;
    if(not result)
    {
      std::println("{}:{} {} [{}] ({})",
                   is_fail_test_ ? "succeed" : "failed",
                   negated_ ? " not" : "",
                   result.expression,
                   result.expanded_expression,
                   _loc_str());
      if(fatal_)
      {
        std::abort();
      }
      if(current_instance != nullptr)
      {
        if constexpr(is_test_class_ and is_evaluator(^^Evaluator))
        {
          // for evaluators: search for symbols in evaluated expression that are present in the parent class
          template for(constexpr auto M : define_static_array(nonstatic_data_members_of(parent_scope_, Ctx)))
          {
            using MemT = [:type_of(M):];
            if constexpr(std::formattable<MemT, char>)
            {
              constexpr auto s = display_string_of(M);
              if(expression_string().find(s) != std::string_view::npos)
              {
                auto parent = static_cast<[:parent_scope_:]*>(current_instance);
                with_extra(expression{std::ref(parent->[:M:]), s});
              }
            }
          }
        }
      }
      std::vector<std::string> context_values;
      detail::base_eval_context::search(expression_string(),
                                        [&](std::string_view name, std::string_view value)
                                        { context_values.push_back(std::format("{} = {}", name, value)); });
      if(not context_values.empty())
      {
        using std::ranges::to;
        using std::views::join_with;
        using namespace std::string_view_literals;
        std::format_to(std::back_inserter(result.expanded_expression),
                       " (with: {})",
                       std::move(context_values) //
                           | join_with(", "sv)   //
                           | to<std::string>());
      }
      if(not extra_descs_.empty())
      {
        using std::ranges::to;
        using std::views::join_with;
        using std::views::transform;
        using namespace std::string_view_literals;
        std::format_to(std::back_inserter(result.expanded_expression),
                       " (with: {})",
                       std::move(extra_descs_)                                    //
                           | transform([](auto&& fn) { return std::move(fn)(); }) //
                           | join_with(", "sv)                                    //
                           | to<std::string>());
      }
    }
    result.should_fail = is_fail_test_;
    result.negated     = negated_;
    get_reporter().template append<Ctx>(std::move(result));
    return true;
  }

  template <typename U> static constexpr auto expected_expression(U const& expected)
  {
    if constexpr(is_expression(^^U))
    {
      return expected.str();
    }
    else if constexpr(decay(^^U) == ^^meta::info)
    {
      return display_string_of(expected);
    }
    else
    {
      return format_expected(expected);
    }
  }

  constexpr decltype(auto) format_value() const
  {
    if constexpr(std::formattable<std::decay_t<value_type>, char>)
    {
      return value();
    }
    else if constexpr(decay(^^value_type) == ^^meta::info)
    {
      return display_string_of(value());
    }
    else
    {
      return "<unformattable>";
    }
  }

  template <typename U> static constexpr decltype(auto) format_expected(U const& expected)
  {
    if constexpr(std::formattable<std::decay_t<U>, char>)
    {
      return expected;
    }
    else if constexpr(decay(^^U) == ^^meta::info)
    {
      return display_string_of(expected);
    }
    else
    {
      return "<unformattable>";
    }
  }

#define _X_TESTING_OPERATIONS(X)   \
  X(==, _is_equal_to)              \
  X(!=, _is_not_equal_to)          \
  X(>, _is_greater_than)           \
  X(>=, _is_greater_or_equal_than) \
  X(<, _is_less_than)              \
  X(<=, _is_less_or_equal_than)

#define X(__op, __name)                                                                \
  template <typename U> constexpr void __name(U const& expected)                       \
  {                                                                                    \
    const auto expected_ex  = [&] { return expected_expression(expected); };           \
    const auto expected_str = [&] { return format_expected(expected); };               \
    validate(                                                                          \
        [&](auto&)                                                                     \
        {                                                                              \
          return detail::validation_result{                                            \
              value() __op expected,                                                   \
              std::format("{} {} {}", expression_string(), #__op, expected_ex()),      \
              std::format("{} {} {} is false", format_value(), #__op, expected_str()), \
          };                                                                           \
        });                                                                            \
  }                                                                                    \
  template <typename U>                                                                \
    requires(^^U != ^^validator)                                                       \
  constexpr decltype(auto) operator __op(U const& expected)                            \
    requires requires() { value() __op expected; }                                     \
  {                                                                                    \
    __name(expected);                                                                  \
    return *this;                                                                      \
  }
  _X_TESTING_OPERATIONS(X)
#undef X

  constexpr decltype(auto) is_empty()
  {
    validate(
        [&](auto&)
        {
          return detail::validation_result{
              value().empty(),
              std::format("{} is empty", expression_string()),
              std::format("{} is not empty", value()),
          };
        });
    return *this;
  }
  constexpr decltype(auto) is_not_empty()
  {
    validate(
        [&](auto&)
        {
          return detail::validation_result{
              not value().empty(),
              std::format("{} is not empty", expression_string()),
              std::format("{} is empty", value()),
          };
        });
    return *this;
  }
  template <typename U> constexpr decltype(auto) contains(U&& expected)
  {
    const auto expected_ex = [&] { return expected_expression(expected); };
    validate(
        [&](auto&)
        {
          return detail::validation_result{
              std::ranges::contains(value(), expected),
              std::format("{} contains {}", expression_string(), expected_ex()),
              std::format("{} does not contain {}", value(), expected),
          };
        });
    return *this;
  }

  constexpr decltype(auto) view()
    requires std::ranges::range<decltype(value())>
  {
    wip_ = false;
    return value();
  }

  template <typename Exception>
    requires(is_evaluator(^^Evaluator)) // not available, use CHECK_THAT_LAZY(...) instead !
  decltype(auto) throws() noexcept
  {
#define _REFLEX_X_KNOWN_EXCEPTIONS(X) \
  X(std::invalid_argument)            \
  X(std::domain_error)                \
  X(std::length_error)                \
  X(std::out_of_range)                \
  X(std::range_error)                 \
  X(std::overflow_error)              \
  X(std::underflow_error)             \
  X(std::bad_alloc)                   \
  X(std::bad_cast)                    \
  X(std::bad_typeid)                  \
  X(std::bad_exception)               \
  X(std::bad_function_call)           \
  X(std::bad_weak_ptr)                \
  X(std::runtime_error)               \
  X(std::logic_error)

    const auto expected_ex = display_string_of(^^Exception);
    validate(
        [&](auto&)
        {
          detail::validation_result result = {
              false,
              std::format("{} throws {}", expression_string(), expected_ex),
          };
          try
          {
            value(); // fetch
            result.expanded_expression = std::format("{} did not throw", expression_string());
          }
          catch(Exception& expected)
          {
            result.result = true;
          }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wexceptions"
#define X(__ex__)                                                                               \
  catch(__ex__ const&)                                                                          \
  {                                                                                             \
    result.expanded_expression =                                                                \
        std::format("{} actually throws {}", expression_string(), display_string_of(^^__ex__)); \
  }
          _REFLEX_X_KNOWN_EXCEPTIONS(X)
#undef X
#pragma GCC diagnostic pop
          catch(...)
          {
            result.expanded_expression = std::format("{} actually throws another exception", expression_string());
          }
          return result;
        });
    return *this;
  }
};
} // namespace detail
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

constexpr detail::__fail_test fail_test;

#define __MAKE_LAZY_ASSERTER(__fatal__, ...)                                                             \
  reflex::testing::_ensure<std::meta::access_context::current()>(                                        \
      reflex::testing::detail::evaluator([&] -> decltype(auto) { return (__VA_ARGS__); }, #__VA_ARGS__), \
      __fatal__)

#define ASSERT_THAT_LAZY(...) __MAKE_LAZY_ASSERTER(true, __VA_ARGS__)
#define CHECK_THAT_LAZY(...)  __MAKE_LAZY_ASSERTER(false, __VA_ARGS__)

#define __MAKE_ASSERTER(__fatal__, ...)                           \
  reflex::testing::_ensure<std::meta::access_context::current()>( \
      reflex::testing::expression((__VA_ARGS__), #__VA_ARGS__),   \
      __fatal__)

#define ASSERT_THAT(...) __MAKE_ASSERTER(true, __VA_ARGS__)
#define CHECK_THAT(...)  __MAKE_ASSERTER(false, __VA_ARGS__)

#define __REFLEXPR(item) ^^item
#define __MAKE_VARIADIC_RELEXPR(...) PP_COMMA_JOIN(__REFLEXPR, __VA_ARGS__)

#define CHECK_CONTEXT(...)                                               \
  const auto __ctx = reflex::testing::detail::eval_context<__MAKE_VARIADIC_RELEXPR(__VA_ARGS__)> \
  {                                                                             \
    __VA_ARGS__                                                                 \
  }

#define STATIC_CHECK_THAT(...) \
  reflex::testing::_static_ensure<(__VA_ARGS__), std::meta::access_context::current()>(#__VA_ARGS__, false)

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
