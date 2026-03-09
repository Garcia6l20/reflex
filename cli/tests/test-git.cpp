#include <doctest/doctest.h>

import reflex.testutils;
import reflex.cli;
import std;

using namespace reflex;

struct[[= cli::command{"Git-like command with subcommands."}]] git
{
  struct[[= cli::command{"Commit changes."}]]
  {
    [[= cli::argument{"Commit message."}]] std::string message;

    int operator()() const
    {
      std::println("Committing with message: {}", message);
      return 0;
    }
  } commit;

  struct[[= cli::command{"Push changes."}]]
  {
    [[= cli::option{"-r/--remote", "Remote name."}]] std::string remote = "origin";

    int operator()() const
    {
      std::println("Pushing to remote: {}", remote);
      return 0;
    }
  } push;
};

using namespace std::string_view_literals;

TEST_CASE("reflex::cli: git")
{
  cli::run(git{}, {"git"sv, "-h"sv});
  cli::run(git{}, {"git"sv, "commit"sv, "-h"sv});
  cli::run(git{}, {"git"sv, "commit"sv, "Initial commit"sv});
  cli::run(git{}, {"git"sv, "push"sv, "-h"sv});
  cli::run(git{}, {"git"sv, "push"sv, "-r"sv, "upstream"sv});

  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(cli::run(git{}, {"git"sv, "commit"sv, "Initial commit"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "Committing with message: Initial commit\n");
  }
}
