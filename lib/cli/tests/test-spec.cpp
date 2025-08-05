#include <reflex/cli.hpp>
#include <reflex/testing_main.hpp>

using namespace reflex;
using namespace reflex::cli;

namespace cli_spec_tests
{
  void test_opt() {
    constexpr auto s = cli::specs{"-f/--force", "Force doing stuff."}.impl();
    STATIC_CHECK_THAT(s.is_opt);
    STATIC_CHECK_THAT(not s.is_field);
    STATIC_CHECK_THAT(s.short_switch == "-f");
    STATIC_CHECK_THAT(s.long_switch == "--force");
    STATIC_CHECK_THAT(s.help == "Force doing stuff.");
    STATIC_CHECK_THAT(s.field.empty());
  }
  void test_opt_field() {
    constexpr auto s = cli::specs{":force:", "-f/--force", "Force doing stuff."}.impl();
    STATIC_CHECK_THAT(s.is_opt);
    STATIC_CHECK_THAT(s.is_field);
    STATIC_CHECK_THAT(s.short_switch == "-f");
    STATIC_CHECK_THAT(s.long_switch == "--force");
    STATIC_CHECK_THAT(s.help == "Force doing stuff.");
    STATIC_CHECK_THAT(s.field == "force");
  }
} // namespace cli_calculator_tests
