#include <doctest/doctest.h>

import reflex.testutils;
import reflex.cli;

using namespace reflex;
using namespace reflex::cli;

struct[[= cli::command{"Simple echo command."}]] echo
{
  [[= cli::argument{"Message to print."}]] std::string            message;
  [[= cli::option{"-p/--prefix", "Prefix."}]] std::string         prefix;
  [[= cli::option{"-r/--repeat", "Repeat count."}.counter()]] int repeat = 1;

  int operator()() const
  {
    for(auto _ : std::views::iota(0, repeat))
    {
      if(!prefix.empty())
        std::print("{}: ", prefix);
      std::println("{}", message);
    }
    return 0;
  }
};

auto run_cli(auto cli, std::initializer_list<std::string_view> args) -> int
{
  auto argv = args
            | std::views::transform([](std::string_view arg) { return arg.data(); })
            | std::ranges::to<std::vector>();
  return cli::run(cli, argv.size(), argv.data());
}

using namespace std::string_view_literals;

TEST_CASE("reflex::cli: opt")
{
  run_cli(echo{}, {"test"sv, "-h"sv});
  run_cli(echo{}, {"test"sv, "hello world"sv});

  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(run_cli(echo{}, {"test"sv, "hello world"sv, "-p"sv, "greeting"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "greeting: hello world\n");
  }

  {
    const auto [out, err] = testutils::capture_out_err(
        [] { //
          CHECK_EQ(run_cli(echo{}, {"test"sv, "hello"sv, "-rr"sv}), 0);
        });
    CHECK(err.empty());
    CHECK_EQ(out, "hello\nhello\nhello\n");
  }
}
