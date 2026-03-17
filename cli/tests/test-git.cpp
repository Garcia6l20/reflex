#include <doctest/doctest.h>

import reflex.cli;
import reflex.testutils;
import git;
import std;

using namespace reflex;

using namespace std::string_view_literals;

static std::vector<std::string> complete(std::string_view comp_line, int comp_point)
{
  // Set the completion env vars, run, capture stdout.
  testutils::set_env("_REFLEX_COMPLETE", "zsh_complete", true);
  testutils::set_env("_REFLEX_COMP_LINE", std::string{comp_line}.c_str(), true);
  testutils::set_env("_REFLEX_COMP_POINT", std::to_string(comp_point).c_str(), true);

  std::string out;
  {
    const auto [o, _] = testutils::capture_out_err([] { cli::run(git{}, {"git"sv}); });
    out               = o;
  }

  testutils::unset_env("_REFLEX_COMPLETE");
  testutils::unset_env("_REFLEX_COMP_LINE");
  testutils::unset_env("_REFLEX_COMP_POINT");

  // Split output into lines.
  std::vector<std::string> results;
  std::istringstream       ss{out};
  std::string              line;
  while(std::getline(ss, line))
    if(not line.empty())
      results.push_back(line);
  return results;
}

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

TEST_CASE("reflex::cli: git completion")
{
  SUBCASE("top-level: empty word lists all subcommands + options")
  {
    auto c = complete("git ", 1);
    CHECK(std::ranges::contains(c, "commit"sv));
    CHECK(std::ranges::contains(c, "push"sv));
    // no options at top-level, only subcommands
    CHECK_FALSE(std::ranges::contains(c, "-h"sv));
    CHECK_FALSE(std::ranges::contains(c, "--help"sv));
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
    CHECK_FALSE(std::ranges::contains(c, "--remote"sv));
  }

  SUBCASE("top-level: partial 'p' completes to 'push'")
  {
    auto c = complete("git p", 1);
    CHECK(std::ranges::contains(c, "push"sv));
    CHECK_FALSE(std::ranges::contains(c, "commit"sv));
    // still no options here
    CHECK_FALSE(std::ranges::contains(c, "-h"sv));
    CHECK_FALSE(std::ranges::contains(c, "--help"sv));
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
    CHECK_FALSE(std::ranges::contains(c, "--remote"sv));
  }

  SUBCASE("top-level: partial 'co' completes to 'commit'")
  {
    auto c = complete("git co", 1);
    CHECK(std::ranges::contains(c, "commit"sv));
    CHECK_FALSE(std::ranges::contains(c, "push"sv));
    // still no options here
    CHECK_FALSE(std::ranges::contains(c, "-h"sv));
    CHECK_FALSE(std::ranges::contains(c, "--help"sv));
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
    CHECK_FALSE(std::ranges::contains(c, "--remote"sv));
  }

  SUBCASE("subcommand push: empty word lists its options")
  {
    auto c = complete("git push -", 2);
    CHECK(std::ranges::contains(c, "--remote"sv));
    CHECK(std::ranges::contains(c, "--help"sv));
    CHECK(std::ranges::contains(c, "-r"sv));
    CHECK(std::ranges::contains(c, "-h"sv));
    // subcommand names of the parent must NOT appear
    CHECK_FALSE(std::ranges::contains(c, "commit"sv));
  }

  SUBCASE("subcommand push: partial '--re' completes to '--remote'")
  {
    auto c = complete("git push --re", 2);
    CHECK(std::ranges::contains(c, "--remote"sv));
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
  }

  SUBCASE("subcommand push: already present option --remote must not be offered again")
  {
    auto c = complete("git push --remote -", 3);
    CHECK(std::ranges::contains(c, "-h"sv));
    CHECK(std::ranges::contains(c, "--help"sv));
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
    CHECK_FALSE(std::ranges::contains(c, "--remote"sv));
  }

  SUBCASE("subcommand commit: empty word lists its options (no sub-options, only --help)")
  {
    auto c = complete("git commit -", 2);
    CHECK(std::ranges::contains(c, "-h"sv));
    CHECK(std::ranges::contains(c, "--help"sv));
    CHECK_FALSE(std::ranges::contains(c, "push"sv));
  }
}