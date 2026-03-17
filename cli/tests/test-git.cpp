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

// Extract just the "value" from completion output lines.
static std::vector<std::string> completion_values(const std::vector<std::string>& lines)
{
  std::vector<std::string> result;
  for(std::size_t i = 1; i < lines.size(); i += 3)
    result.push_back(lines[i]);
  return result;
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

TEST_CASE("reflex::cli: git completion - branch subcommand")
{
  SUBCASE("empty argument: all branch fixtures offered")
  {
    auto v = completion_values(complete("git branch ", 2));
    CHECK(std::ranges::contains(v, "main"sv));
    CHECK(std::ranges::contains(v, "master"sv));
    CHECK(std::ranges::contains(v, "develop"sv));
    CHECK(std::ranges::contains(v, "feature/foo"sv));
    CHECK(std::ranges::contains(v, "feature/bar"sv));
    CHECK(std::ranges::contains(v, "hotfix/123"sv));
  }

  SUBCASE("partial 'feat' filters correctly")
  {
    auto v = completion_values(complete("git branch feat", 2));
    CHECK(std::ranges::contains(v, "feature/foo"sv));
    CHECK(std::ranges::contains(v, "feature/bar"sv));
    CHECK_FALSE(std::ranges::contains(v, "main"sv));
    CHECK_FALSE(std::ranges::contains(v, "develop"sv));
  }

  SUBCASE("partial 'main' returns only 'main'")
  {
    auto v = completion_values(complete("git branch main", 2));
    CHECK(std::ranges::contains(v, "main"sv));
    CHECK_FALSE(std::ranges::contains(v, "master"sv));
  }

  SUBCASE("partial 'ma' returns 'main' and 'master'")
  {
    auto v = completion_values(complete("git branch ma", 2));
    CHECK(std::ranges::contains(v, "main"sv));
    CHECK(std::ranges::contains(v, "master"sv));
    CHECK_FALSE(std::ranges::contains(v, "develop"sv));
  }

  SUBCASE("option '--' still offered alongside branch names")
  {
    auto v = completion_values(complete("git branch -", 2));
    CHECK(std::ranges::contains(v, "--delete"sv));
    CHECK(std::ranges::contains(v, "-d"sv));
    CHECK(std::ranges::contains(v, "--move"sv));
    CHECK(std::ranges::contains(v, "-m"sv));
  }
}

TEST_CASE("reflex::cli: git completion - push subcommand with custom option completer")
{
  SUBCASE("empty value after -r: all remotes offered")
  {
    auto v = completion_values(complete("git push -r ", 3));
    CHECK(std::ranges::contains(v, "origin"sv));
    CHECK(std::ranges::contains(v, "upstream"sv));
    CHECK(std::ranges::contains(v, "fork"sv));
  }

  SUBCASE("partial 'up' after -r filters to 'upstream'")
  {
    auto v = completion_values(complete("git push -r up", 3));
    CHECK(std::ranges::contains(v, "upstream"sv));
    CHECK_FALSE(std::ranges::contains(v, "origin"sv));
    CHECK_FALSE(std::ranges::contains(v, "fork"sv));
  }

  SUBCASE("partial 'or' after --remote filters to 'origin'")
  {
    auto v = completion_values(complete("git push --remote or", 3));
    CHECK(std::ranges::contains(v, "origin"sv));
    CHECK_FALSE(std::ranges::contains(v, "upstream"sv));
  }
}