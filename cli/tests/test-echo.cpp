#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

using namespace reflex;

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

struct[[= cli::command{"Add command."}]] add
{
  [[= cli::argument{"LHS."}]] int lhs;
  [[= cli::argument{"RHS."}]] int rhs;

  int operator()() const
  {
    std::println("{}", lhs + rhs);
    return 0;
  }
};

using namespace std::string_view_literals;

TEST_CASE("reflex::cli: echo")
{
  cli::run(echo{}, {"echo"sv, "-h"sv});
  cli::run(echo{}, {"echo"sv, "hello world"sv});

  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(cli::run(echo{}, {"echo"sv, "hello world"sv, "-p"sv, "greeting"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "greeting: hello world\n");
  }

  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(cli::run(echo{}, {"echo"sv, "hello"sv, "-rr"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "hello\nhello\nhello\n");
  }
}

TEST_CASE("reflex::cli: add")
{
  SUBCASE("integer addition works correctly")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run(add{}, {"add"sv, "2"sv, "3"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "5\n");
  }
  SUBCASE("negative number handled correctly")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run(add{}, {"add"sv, "-2"sv, "-3"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "-5\n");
  }
}
