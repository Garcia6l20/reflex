#pragma once

#include <reflex/meta.hpp>

#include <map>
#include <print>
#include <source_location>
#include <string>

namespace reflex::testing::detail
{
struct validation_result
{
  bool                 result;
  std::string          expression;
  std::string          expanded_expression;
  std::source_location location;
  bool                 should_fail = false;
  bool                 negated     = false;

  operator bool() const
  {
    return result;
  }
};

struct __fail_test
{
};

consteval bool is_fail_test(meta::info R)
{
  return meta::has_annotation(R, ^^__fail_test);
}

struct reporter_t
{
  void append(std::string const& test_suite, std::string const& test_case, validation_result&& result)
  {
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
  template <auto ctx> void append(validation_result&& result)
  {
    constexpr auto scope      = ctx.scope();
    auto           test_suite = std::string{display_string_of(parent_of(scope))};
    auto           test_case  = std::string{display_string_of(scope)};
    append(test_suite, test_case, std::move(result));
  }

  int finish() const
  {
    using std::ranges::any_of;
    using std::ranges::count_if;
    using std::views::filter;

    struct counter
    {
      size_t fail_count    = 0;
      size_t success_count = 0;

      void failed()
      {
        ++fail_count;
      }
      void succeed()
      {
        ++success_count;
      }
      size_t total() const
      {
        return fail_count + success_count;
      }
      inline bool operator()(bool value)
      {
        if(value)
        {
          succeed();
        }
        else
        {
          failed();
        }
        return value;
      }
    };
    counter suite_counter{};
    counter case_counter{};
    counter check_counter{};

    auto failed_suites = results //
                         | filter(
                               [&](auto const& test_cases)
                               {
                                 return !suite_counter(count_if(test_cases.second, [&](auto const& test_case) {//
                                   return !case_counter(count_if(test_case.second, [&](auto const& result) { return not check_counter(result); }) == 0);
                                 }) == 0);
                               });

    for(const auto& [suite, test_cases] : failed_suites)
    {
      std::println("❌ suite '{}' failed:", suite);

      auto failed_cases = test_cases //
                          | filter([](auto const& test_case)
                                   { return any_of(test_case.second, [](auto const& result) { return not result; }); });
      for(const auto& [test_case, res_vec] : failed_cases)
      {
        std::println("  ❌ case '{}' failed:", test_case);

        auto failed_results = res_vec //
                              | filter([](auto const& result) { return not result; });
        for(const validation_result& result : failed_results)
        {
          std::println("    ❌ {}:{} {}",
                       result.should_fail ? "fail-check succeed" : "check failed",
                       result.negated ? " not" : "",
                       result.expression);
          if(not result.expanded_expression.empty() and result.expression != result.expanded_expression)
          {
            std::println("                 {}👉 {} {}",
                         result.should_fail ? "      " : "",
                         result.negated ? " not" : "",
                         result.expanded_expression);
          }
          std::println("       {}:{}", result.location.file_name(), result.location.line());
        }
      }
    }

    std::println("{}", suite_counter.fail_count ? "❌ failed" : "✔️  success");

    const auto pass_align =
        1 +
        std::ranges::max({suite_counter.success_count, case_counter.success_count, check_counter.success_count}) / 10;
    const auto fail_align =
        1 + std::ranges::max({suite_counter.fail_count, case_counter.fail_count, check_counter.fail_count}) / 10;
    const auto total_align =
        1 + std::ranges::max({suite_counter.total(), case_counter.total(), check_counter.total()}) / 10;

    auto print_stats = [&](auto id, auto const& counter)
    {
      std::println(" 👉 {}: {:>{}} pass, {:>{}} failed, {:>{}} total ({}% pass)",
                   id,
                   counter.success_count,
                   pass_align,
                   counter.fail_count,
                   fail_align,
                   counter.total(),
                   total_align,
                   std::round(100 * counter.success_count / float(counter.total())));
    };
    print_stats("suites", suite_counter);
    print_stats(" cases", case_counter);
    print_stats("checks", check_counter);
    return check_counter.fail_count;
  }

  std::map<std::string, std::map<std::string, std::vector<validation_result>>> results;
};

reporter_t& get_reporter();
} // namespace reflex::testing::detail