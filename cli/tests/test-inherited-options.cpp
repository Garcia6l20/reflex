#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

using namespace reflex;
using namespace std::string_view_literals;

/** @file
 * @brief options and arguments declared in a base class
 *
 * The case this exists for is a command with sub-commands that must each accept
 * the same set of options. Parent options have to come before the verb, which
 * nobody types, so the options belong on every sub-command, and writing them
 * out once per sub-command is how they drift apart.
 */

namespace
{
struct shared_options
{
  [[= cli::option{"--shared-flag", "Declared in a base class."}.flag()]] bool shared_flag = false;

  [[= cli::option{"--shared-value", "Also declared in a base class."}]] //
  std::string shared_value = "none";
};

struct[[= cli::command{"Inherits its options."}]] inheriting : shared_options
{
  [[= cli::option{"--own-flag", "Declared directly."}.flag()]] bool own_flag = false;

  int operator()() const
  {
    std::println("{} {} {}", shared_flag, shared_value, own_flag);
    return 0;
  }
};

/// @brief two levels, so the walk has to recurse rather than look one up
struct more_shared_options
{
  [[= cli::option{"--deep-flag", "Two bases up."}.flag()]] bool deep_flag = false;
};

struct middle_options : more_shared_options
{
  [[= cli::option{"--middle-flag", "One base up."}.flag()]] bool middle_flag = false;
};

struct[[= cli::command{"Inherits through two levels."}]] deep : middle_options
{
  int operator()() const
  {
    std::println("{} {}", deep_flag, middle_flag);
    return 0;
  }
};

/// @brief the reason this matters, a sub-command carrying the shared options
struct[[= cli::command{"Has sub-commands."}]] parent
{
  struct[[= cli::command{"First."}]] : shared_options
  {
    int operator()() const
    {
      std::println("one {} {}", shared_flag, shared_value);
      return 0;
    }
  } one;

  struct[[= cli::command{"Second."}]] : shared_options
  {
    int operator()() const
    {
      std::println("two {} {}", shared_flag, shared_value);
      return 0;
    }
  } two;
};
} // namespace

TEST_CASE("an option declared in a base class is parsed")
{
  const auto [out, err] = testutils::capture_out_err([] {
    CHECK_EQ(
        cli::run(
            inheriting{},
            {"inheriting"sv, "--shared-flag"sv, "--shared-value"sv, "set"sv, "--own-flag"sv}),
        0);
  });
  CHECK(err.empty());
  CHECK_EQ(out, "true set true\n");
}

TEST_CASE("an inherited option keeps its default when it is not given")
{
  const auto [out, err] =
      testutils::capture_out_err([] { CHECK_EQ(cli::run(inheriting{}, {"inheriting"sv}), 0); });
  CHECK(err.empty());
  CHECK_EQ(out, "false none false\n");
}

TEST_CASE("an inherited option appears in the help text")
{
  const auto [out, err] =
      testutils::capture_out_err([] { cli::run(inheriting{}, {"inheriting"sv, "--help"sv}); });
  CHECK(out.find("--shared-flag") != std::string::npos);
  CHECK(out.find("Declared in a base class.") != std::string::npos);
  CHECK(out.find("--own-flag") != std::string::npos);
}

TEST_CASE("the walk recurses through more than one base")
{
  const auto [out, err] = testutils::capture_out_err(
      [] { CHECK_EQ(cli::run(deep{}, {"deep"sv, "--deep-flag"sv, "--middle-flag"sv}), 0); });
  CHECK(err.empty());
  CHECK_EQ(out, "true true\n");
}

TEST_CASE("two sub-commands share one set of options without repeating them")
{
  {
    const auto [out, err] = testutils::capture_out_err([] {
      CHECK_EQ(cli::run(parent{}, {"parent"sv, "one"sv, "--shared-value"sv, "a"sv}), 0);
    });
    CHECK(err.empty());
    CHECK_EQ(out, "one false a\n");
  }
  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(cli::run(parent{}, {"parent"sv, "two"sv, "--shared-flag"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "two true none\n");
  }
}

TEST_CASE("an inherited option is accepted after the sub-command name")
{
  // The whole point. A parent option has to precede the verb, so an option that
  // belongs to the sub-command is the only one a user can type where they
  // expect to.
  const auto [out, err] = testutils::capture_out_err(
      [] { CHECK_EQ(cli::run(parent{}, {"parent"sv, "one"sv, "--shared-flag"sv}), 0); });
  CHECK(err.empty());
  CHECK_EQ(out, "one true none\n");
}
