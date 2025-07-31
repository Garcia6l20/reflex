#pragma once

#include <reflex/cli.hpp>
#include <reflex/testing.hpp>

namespace reflex::testing
{
template <meta::info Ns>
struct[[= cli::specs<"reflex test runner.">]] //
    command_line : cli::command
{
  [[= cli::specs<"-v/--verbose", "Show more details.">, = cli::_count]] //
      int verbose = false;

  [[= cli::specs<"Run tests.">, = cli::_default]]                      //
      [[= cli::specs<":match:", "-m/--match", "Run matching tests.">]] //
      int
      exec(std::optional<std::string_view> match) const noexcept
  {
    constexpr auto suites = define_static_array(get_suites(Ns));
    static_assert(not suites.empty(), "No test suite found !");
    template for(constexpr auto suite : suites)
    {
      template for(constexpr auto caze : define_static_array(get_cases(suite)))
      {
        if(match.has_value())
        {
          using namespace std::string_view_literals;
          const auto v = std::views::split(match.value(), ":"sv) | std::ranges::to<std::vector>();
          if(v.size() != 2)
          {
            throw std::runtime_error(std::format("invalid matcher: {}", v));
          }
          if(std::string_view{v[0]} != identifier_of(suite) or std::string_view{v[1]} != identifier_of(caze))
          {
            continue;
          }
        }
        if constexpr(is_function(caze))
        {
          std::println("Running: {}...", identifier_of(caze));
          [:caze:]();
        }
        else
        {
          using CaseT = [:caze:];

          std::print("Running: {}... ", identifier_of(caze));
          static constexpr auto functions =
              define_static_array(meta::member_functions_of(caze) |
                                  std::views::filter([](auto R) { return identifier_of(R).contains("test"); }));
          template for(constexpr auto ii : std::views::iota(0uz, functions.size()))
          {
            constexpr auto fn = functions[ii];
            std::print("{}", identifier_of(fn));
            auto obj = CaseT{};
            obj.[:fn:]();
            if constexpr(ii < functions.size() - 1)
            {
              std::print(", ");
            }
          }
          std::println();
        }
      }
    }
    return detail::reporter.finish();
  }

  [[= cli::specs<"List tests.">]] //
      int
      ls() const noexcept
  {
    constexpr auto suites = define_static_array(get_suites(Ns));
    static_assert(not suites.empty(), "No test suite found !");
    template for(constexpr auto suite : suites)
    {
      constexpr auto suite_loc = source_location_of(suite);
      std::println("suite: {} - {}:{}", display_string_of(suite), suite_loc.file_name(), suite_loc.line());
      template for(constexpr auto caze : define_static_array(get_cases(suite)))
      {
        constexpr auto caze_loc = source_location_of(caze);

        if constexpr(is_function(caze))
        {
          std::println(" - case: {} - {}:{}", display_string_of(caze), caze_loc.file_name(), caze_loc.line());
        }
        else
        {
          static constexpr auto functions =
              define_static_array(meta::member_functions_of(caze) |
                                  std::views::filter([](auto R) { return identifier_of(R).contains("test"); }));
          template for(constexpr auto fn : functions)
          {
            std::println(" - case: {}.{} - {}:{}",
                         display_string_of(caze),
                         display_string_of(fn),
                         caze_loc.file_name(),
                         caze_loc.line());
          }
        }
      }
    }
    return 0;
  }

  [[= cli::specs<"Discover tests.">]] //
      int
      discover() const noexcept
  {
    constexpr auto suites = define_static_array(get_suites(Ns));
    static_assert(not suites.empty(), "No test suite found !");
    template for(constexpr auto suite : suites)
    {
      template for(constexpr auto caze : define_static_array(get_cases(suite)))
      {
        if constexpr(is_function(caze))
        {
          std::print("{}:{}", display_string_of(suite), display_string_of(caze));
          if constexpr(meta::has_annotation(caze, ^^fail_test))
          {
            std::print(":failing");
          }
          std::println();
        }
        else
        {
          static constexpr auto functions =
              define_static_array(meta::member_functions_of(caze) |
                                  std::views::filter([](auto R) { return identifier_of(R).contains("test"); }));
          template for(constexpr auto fn : functions)
          {
            std::print("{}:{}.{}", display_string_of(suite), display_string_of(caze), display_string_of(fn));
            if constexpr(meta::has_annotation(caze, ^^fail_test))
            {
              std::print(":failing");
            }
            std::println();
          }
        }
      }
    }
    return 0;
  }
};
} // namespace reflex::testing
