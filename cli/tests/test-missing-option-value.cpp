#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

using namespace reflex;
using namespace std::string_view_literals;

// An option that takes a value, given as the last element of argv, read one
// past the end of the range and crashed the process. The end check was present
// and ran one line after the dereference rather than before it. Every option
// shape that takes a value was affected, which is why there is one case per
// shape here rather than one case.

namespace
{
enum class[[= derive(Format, Parse)]] mode
{
  fast,
  slow,
};

struct[[= cli::command{"Every option shape that takes a value."}]] shapes
{
  [[= cli::option{"-r/--required", "A value with no default."}]] //
  std::string required;

  [[= cli::option{"-o/--optional", "A value that may be absent."}]] //
  std::optional<std::string> optional;

  [[= cli::option{"-n/--number", "A value that is parsed."}]] //
  std::optional<std::int64_t> number;

  [[= cli::option{"-m/--mode", "A value matched against an enum."}]] //
  mode chosen = mode::fast;

  [[= cli::option{"-l/--list", "A value that may repeat."}]] //
  std::vector<std::string> list;

  [[= cli::option{"-c/--count", "A counter, which takes no value."}.counter()]] //
  int count = 0;

  [[= cli::option{"-f/--flag", "A flag, which takes no value."}.flag()]] //
  bool flag = false;

  int operator()() const
  {
    return 0;
  }
};

struct[[= cli::command{"A sub-command with a value-taking option."}]] child
{
  [[= cli::option{"-v/--value", "A value with no default."}]] //
  std::string value;

  int operator()() const
  {
    return 0;
  }
};

struct[[= cli::command{"A parent carrying a sub-command."}]] parent
{
  [[= cli::option{"-t/--top", "A value on the parent."}]] //
  std::string top;

  child sub;
};

/// @return the exit status and what went to stderr
auto run_shapes(std::initializer_list<std::string_view> args)
{
  int  rc = -1;
  auto captured =
      testutils::capture_out_err([&] { rc = cli::run(shapes{}, args); });
  return std::pair{rc, captured.second};
}
} // namespace

TEST_CASE("reflex::cli: an option given no value is refused rather than fatal")
{
  // Each of these read past the end of argv before the fix. The assertion that
  // matters is that the process survives to return a status at all.
  SUBCASE("a required value")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "--required"sv});
    CHECK(rc != 0);
    CHECK(err.find("missing value for option: --required") != std::string::npos);
  }
  SUBCASE("an optional value")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "--optional"sv});
    CHECK(rc != 0);
    CHECK(err.find("missing value for option: --optional") != std::string::npos);
  }
  SUBCASE("a parsed value")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "--number"sv});
    CHECK(rc != 0);
    CHECK(err.find("missing value for option: --number") != std::string::npos);
  }
  SUBCASE("an enum value")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "--mode"sv});
    CHECK(rc != 0);
    CHECK(err.find("missing value for option: --mode") != std::string::npos);
  }
  SUBCASE("a repeatable value")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "--list"sv});
    CHECK(rc != 0);
    CHECK(err.find("missing value for option: --list") != std::string::npos);
  }
  SUBCASE("the short spelling behaves the same")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "-r"sv});
    CHECK(rc != 0);
    CHECK(err.find("missing value for option: -r") != std::string::npos);
  }
}

TEST_CASE("reflex::cli: an option that takes no value is unaffected at the end of argv")
{
  // A counter and a flag consume nothing, so neither ever advanced the iterator
  // and neither could crash. Asserted so that a fix to the value path cannot
  // quietly break the paths that were already correct.
  SUBCASE("a counter")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "--count"sv});
    CHECK(rc == 0);
    CHECK(err.empty());
  }
  SUBCASE("a flag")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "--flag"sv});
    CHECK(rc == 0);
    CHECK(err.empty());
  }
  SUBCASE("a bundled counter")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "-ccc"sv});
    CHECK(rc == 0);
    CHECK(err.empty());
  }
}

TEST_CASE("reflex::cli: a value still parses when it is there")
{
  // The other half of the fix. Moving the end check before the dereference must
  // not change what happens when the value is present.
  SUBCASE("the last element of argv is a value")
  {
    int  rc = -1;
    auto captured = testutils::capture_out_err(
        [&] { rc = cli::run(shapes{}, {"shapes"sv, "--required"sv, "here"sv}); });
    CHECK(rc == 0);
    CHECK(captured.second.empty());
  }
  SUBCASE("a value that does not parse is still reported as invalid, not missing")
  {
    const auto [rc, err] = run_shapes({"shapes"sv, "--number"sv, "seven"sv});
    CHECK(rc != 0);
    CHECK(err.find("invalid value for option") != std::string::npos);
    CHECK(err.find("missing value") == std::string::npos);
  }
  SUBCASE("a value that looks like an option is still taken as the value")
  {
    int  rc = -1;
    auto captured = testutils::capture_out_err(
        [&] { rc = cli::run(shapes{}, {"shapes"sv, "--required"sv, "--optional"sv}); });
    CHECK(rc == 0);
  }
}

TEST_CASE("reflex::cli: a sub-command's option is refused the same way at the end of argv")
{
  SUBCASE("the option belongs to the sub-command")
  {
    int  rc = -1;
    auto captured = testutils::capture_out_err(
        [&] { rc = cli::run(parent{}, {"parent"sv, "sub"sv, "--value"sv}); });
    CHECK(rc != 0);
    CHECK(captured.second.find("missing value for option: --value") != std::string::npos);
  }
  SUBCASE("the option belongs to the parent and the sub-command follows nothing")
  {
    int  rc = -1;
    auto captured =
        testutils::capture_out_err([&] { rc = cli::run(parent{}, {"parent"sv, "--top"sv}); });
    CHECK(rc != 0);
    CHECK(captured.second.find("missing value for option: --top") != std::string::npos);
  }
  SUBCASE("a sub-command as the last element of argv is not a missing value")
  {
    int  rc = -1;
    auto captured =
        testutils::capture_out_err([&] { rc = cli::run(parent{}, {"parent"sv, "sub"sv}); });
    CHECK(captured.second.find("missing value") == std::string::npos);
    CHECK(rc == 0);
  }
}
