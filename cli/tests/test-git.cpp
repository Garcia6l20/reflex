#include <doctest/doctest.h>

#include <reflex/cli.hpp>

#include <git.hpp>
#include <testutils.hpp>

#include <sstream>

using namespace reflex;
using namespace std::string_view_literals;
using namespace testutils;

TEST_CASE("reflex::cli: git")
{
  auto print_run = [](auto&&... args) {
    auto [rc, out, err] = run<git>(std::forward<decltype(args)>(args)...);
    std::println("=== git {} ===", std::inplace_vector<std::string_view, sizeof...(args)>{args...});
    std::println("rc: {}", rc);
    std::println("out: {}", out);
    std::println("err: {}", err);
  };
  print_run("-h");
  print_run("commit", "-h");
  print_run("commit", "Initial commit");
  print_run("push", "-h");
  print_run("push", "-r", "upstream");

  SUBCASE("commit")
  {
    const auto [rc, out, err] = run<git>("commit", "Initial commit");
    CHECK_EQ(rc, 0);
    // CHECK(err == "going to execute: commit...\n");
    CHECK_EQ(out, "Committing with message: Initial commit\n");
  }
  SUBCASE("add")
  {
    const auto [rc, out, err] = run<git>("add", "test", "data");
    // CHECK(err == "going to execute: add...\n");
    CHECK_EQ(out, "Adding: test\nAdding: data\n");
  }
  SUBCASE("push with remote and yes")
  {
    const auto [rc, out, err] = run<git>("push", "-r", "origin", "-y");
    CHECK_EQ(rc, 0);
    // CHECK(err == "going to execute: push...\n");
    CHECK_EQ(out, "Pushing to remote: origin (no confirmation)\n");
  }
}

TEST_CASE("reflex::cli: git completion")
{
  SUBCASE("top-level: empty word lists all subcommands + options")
  {
    auto c = complete<git>("git ", 1);
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
    auto c = complete<git>("git p", 1);
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
    auto c = complete<git>("git co", 1);
    CHECK(std::ranges::contains(c, "commit"sv));
    CHECK_FALSE(std::ranges::contains(c, "push"sv));
    // still no options here
    CHECK_FALSE(std::ranges::contains(c, "-h"sv));
    CHECK_FALSE(std::ranges::contains(c, "--help"sv));
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
    CHECK_FALSE(std::ranges::contains(c, "--remote"sv));
  }

  SUBCASE("subcommand push: a complete completion returns the completion")
  {
    auto c = complete<git>("git push", 1);
    CHECK(std::ranges::contains(c, "push"sv));
    CHECK_FALSE(std::ranges::contains(c, "--help"sv));
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
    CHECK_FALSE(std::ranges::contains(c, "-h"sv));
    CHECK_FALSE(std::ranges::contains(c, "commit"sv));
  }

  SUBCASE("subcommand push: a complete completion returns the completion")
  {
    auto c = complete<git>("git push", 2);
    CHECK_FALSE(std::ranges::contains(c, "push"sv));
    CHECK(std::ranges::contains(c, "--help"sv));
    CHECK(std::ranges::contains(c, "-r"sv));
    CHECK(std::ranges::contains(c, "-h"sv));
  }

  SUBCASE("subcommand push: empty word lists its options")
  {
    auto c = complete<git>("git push -", 2);
    CHECK(std::ranges::contains(c, "--remote"sv));
    CHECK(std::ranges::contains(c, "--help"sv));
    CHECK(std::ranges::contains(c, "-r"sv));
    CHECK(std::ranges::contains(c, "-h"sv));
    // subcommand names of the parent must NOT appear
    CHECK_FALSE(std::ranges::contains(c, "commit"sv));
  }

  SUBCASE("subcommand push: complete '-r' completes to '-r'")
  {
    auto c = complete<git>("git push -r", 2);
    CHECK(std::ranges::contains(c, "-r"sv));
  }

  SUBCASE("subcommand push: complete '-r' completes to '-r'")
  {
    auto c = complete<git>("git push -r", 3);
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
  }

  SUBCASE("subcommand push: partial '--re' completes to '--remote'")
  {
    auto c = complete<git>("git push --re", 2);
    CHECK(std::ranges::contains(c, "--remote"sv));
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
  }

  SUBCASE("subcommand push: already present option --remote must not be offered again")
  {
    auto c = complete<git>("git push --remote origin -", 4);
    CHECK(std::ranges::contains(c, "-h"sv));
    CHECK(std::ranges::contains(c, "--help"sv));
    CHECK_FALSE(std::ranges::contains(c, "-r"sv));
    CHECK_FALSE(std::ranges::contains(c, "--remote"sv));
  }

  SUBCASE("subcommand commit: empty word lists its options (no sub-options, only --help)")
  {
    auto c = complete<git>("git commit -", 2);
    CHECK(std::ranges::contains(c, "-h"sv));
    CHECK(std::ranges::contains(c, "--help"sv));
    CHECK_FALSE(std::ranges::contains(c, "push"sv));
  }
}

TEST_CASE("reflex::cli: git completion - branch subcommand")
{
  SUBCASE("empty argument: all branch fixtures offered")
  {
    auto v = completion_values(complete<git>("git branch ", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "main"sv));
    CHECK(std::ranges::contains(v, "master"sv));
    CHECK(std::ranges::contains(v, "develop"sv));
    CHECK(std::ranges::contains(v, "feature/foo"sv));
    CHECK(std::ranges::contains(v, "feature/bar"sv));
    CHECK(std::ranges::contains(v, "hotfix/123"sv));
  }

  SUBCASE("partial 'feat' filters correctly")
  {
    auto v = completion_values(complete<git>("git branch feat", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "feature/foo"sv));
    CHECK(std::ranges::contains(v, "feature/bar"sv));
    CHECK_FALSE(std::ranges::contains(v, "main"sv));
    CHECK_FALSE(std::ranges::contains(v, "develop"sv));
  }

  SUBCASE("partial 'main' returns only 'main'")
  {
    auto v = completion_values(complete<git>("git branch main", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "main"sv));
    CHECK_FALSE(std::ranges::contains(v, "master"sv));
  }

  SUBCASE("partial 'ma' returns 'main' and 'master'")
  {
    auto v = completion_values(complete<git>("git branch ma", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "main"sv));
    CHECK(std::ranges::contains(v, "master"sv));
    CHECK_FALSE(std::ranges::contains(v, "develop"sv));
  }

  SUBCASE("option '--' still offered alongside branch names")
  {
    auto v = completion_values(complete<git>("git branch -", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
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
    auto v = completion_values(complete<git>("git push -r ", 3))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "origin"sv));
    CHECK(std::ranges::contains(v, "upstream"sv));
    CHECK(std::ranges::contains(v, "fork"sv));
  }

  SUBCASE("complete upstream should not repeat")
  {
    auto v = completion_values(complete<git>("git push -r upstream", 3))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK_FALSE(std::ranges::contains(v, "upstream"sv));
    CHECK_FALSE(std::ranges::contains(v, "origin"sv));
    CHECK_FALSE(std::ranges::contains(v, "fork"sv));
  }

  SUBCASE("partial 'up' after -r filters to 'upstream'")
  {
    auto v = completion_values(complete<git>("git push -r up", 3))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "upstream"sv));
    CHECK_FALSE(std::ranges::contains(v, "origin"sv));
    CHECK_FALSE(std::ranges::contains(v, "fork"sv));
  }

  SUBCASE("partial 'or' after --remote filters to 'origin'")
  {
    auto v = completion_values(complete<git>("git push --remote or", 4))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "origin"sv));
    CHECK_FALSE(std::ranges::contains(v, "upstream"sv));
  }

  SUBCASE("trailing --yes after 'origin' is handled correctly")
  {
    auto v = completion_values(complete<git>("git push --remote origin --yes", 5))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "--help"sv));
    CHECK_FALSE(std::ranges::contains(v, "--yes"sv));
  }
}

TEST_CASE("reflex::cli: git completion - add-text-file")
{
  SUBCASE("option with path completer offers file system paths")
  {
    auto v = completion_values(complete<git>("git add-text-file ", 2));
    CHECK(v.size() == 1);
    CHECK(v.at(0).type == cli::completion_type::file);
    CHECK(v.at(0).value == "*.txt"sv);
  }
}

TEST_CASE("reflex::cli: git completion - diff")
{
  SUBCASE("complete first argument")
  {
    auto v = completion_values(complete<git>("git diff ", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "master"sv));
    CHECK(std::ranges::contains(v, "develop"sv));
    CHECK(std::ranges::contains(v, "feature/foo"sv));
    CHECK(std::ranges::contains(v, "feature/bar"sv));
  }
  SUBCASE("complete partial first argument")
  {
    auto v = completion_values(complete<git>("git diff ma", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "master"sv));
    CHECK(std::ranges::contains(v, "main"sv));
  }
  SUBCASE("complete second argument")
  {
    auto v = completion_values(complete<git>("git diff master ", 3))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "develop"sv));
    CHECK(std::ranges::contains(v, "feature/foo"sv));
    CHECK(std::ranges::contains(v, "feature/bar"sv));
  }
  SUBCASE("complete partial second argument")
  {
    auto v = completion_values(complete<git>("git diff master de", 3))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "develop"sv));
  }
}

TEST_CASE("reflex::cli: git completion - show")
{
  SUBCASE("complete first argument")
  {
    auto v = completion_values(complete<git>("git show ", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "origin"sv));
    CHECK(std::ranges::contains(v, "upstream"sv));
    CHECK(std::ranges::contains(v, "fork"sv));
  }
  SUBCASE("complete partial first argument")
  {
    auto v = completion_values(complete<git>("git show or", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "origin"sv));
  }
  SUBCASE("complete second argument")
  {
    auto v = completion_values(complete<git>("git show origin ", 3))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "develop"sv));
    CHECK(std::ranges::contains(v, "feature/foo"sv));
    CHECK(std::ranges::contains(v, "feature/bar"sv));
  }
  SUBCASE("complete partial second argument")
  {
    auto v = completion_values(complete<git>("git show origin de", 3))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "develop"sv));
  }
}

TEST_CASE("reflex::cli: git completion - log-level")
{
  SUBCASE("complete first argument")
  {
    auto v = completion_values(complete<git>("git log-level ", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "verbose"sv));
    CHECK(std::ranges::contains(v, "debug"sv));
    CHECK(std::ranges::contains(v, "info"sv));
    CHECK(std::ranges::contains(v, "warning"sv));
    CHECK(std::ranges::contains(v, "error"sv));
  }
  SUBCASE("complete partial first argument")
  {
    auto v = completion_values(complete<git>("git log-level de", 2))
           | std::views::transform(&cli::completion<>::value)
           | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(v, "debug"sv));
    CHECK_FALSE(std::ranges::contains(v, "verbose"sv));
    CHECK_FALSE(std::ranges::contains(v, "info"sv));
  }
}
