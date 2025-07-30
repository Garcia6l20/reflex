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
        std::println("Running: {}...", identifier_of(caze));
        [:caze:]();
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
        std::println(" - case: {} - {}:{}", display_string_of(caze), caze_loc.file_name(), caze_loc.line());
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
        std::print("{}:{}", display_string_of(suite), display_string_of(caze));
        if constexpr(meta::has_annotation(caze, ^^fail_test))
        {
          std::print(":failing");
        }
        std::println();
      }
    }
    return 0;
  }
};
} // namespace reflex::testing

int main(int argc, const char** argv)
{
  reflex::testing::command_line<^^::> cli{};
  return cli.run(argc, argv);
}
