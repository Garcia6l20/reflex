#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

#include <string>
#include <vector>

using namespace reflex;
using namespace testutils;
using namespace std::string_view_literals;

namespace
{
std::vector<int> collected_options{};

struct[[= cli::command{"An option that parses and repeats."}]] numbers
{
  [[= cli::option{"-n/--number", "A number, repeatable."}]] //
  std::vector<int> values{};

  int operator()() const
  {
    collected_options = values;
    return 0;
  }
};

std::vector<int> collected_arguments{};

struct[[= cli::command{"A trailing argument that parses and repeats."}]] tally
{
  [[= cli::argument{"Numbers to add up."}]] //
  std::vector<int> values{};

  int operator()() const
  {
    collected_arguments = values;
    return 0;
  }
};

auto run_numbers(std::initializer_list<std::string_view> args)
{
  int  rc       = -1;
  auto captured = capture_out_err([&] { rc = cli::run(numbers{}, args); });
  return std::pair{rc, captured.second};
}

auto run_tally(std::initializer_list<std::string_view> args)
{
  int  rc       = -1;
  auto captured = capture_out_err([&] { rc = cli::run(tally{}, args); });
  return std::pair{rc, captured.second};
}
} // namespace

TEST_CASE("reflex::cli: a repeatable option parses every value it is given")
{
  collected_options.clear();
  const auto [rc, err] = run_numbers({"numbers"sv, "-n"sv, "1"sv, "--number"sv, "2"sv});
  CHECK_EQ(rc, 0);
  CHECK(err.empty());
  CHECK_EQ(collected_options, std::vector{1, 2});
}

TEST_CASE("reflex::cli: a repeatable option refuses a value that does not parse")
{
  const auto [rc, err] = run_numbers({"numbers"sv, "-n"sv, "1"sv, "-n"sv, "two"sv});
  CHECK_NE(rc, 0);
  CHECK_NE(err.find("invalid value for option"), std::string::npos);
}

TEST_CASE("reflex::cli: a trailing argument parses every value it is given")
{
  collected_arguments.clear();
  const auto [rc, err] = run_tally({"tally"sv, "3"sv, "4"sv, "5"sv});
  CHECK_EQ(rc, 0);
  CHECK(err.empty());
  CHECK_EQ(collected_arguments, std::vector{3, 4, 5});
}

TEST_CASE("reflex::cli: a trailing argument refuses a value that does not parse")
{
  const auto [rc, err] = run_tally({"tally"sv, "3"sv, "four"sv});
  CHECK_NE(rc, 0);
  CHECK_NE(err.find("invalid argument value"), std::string::npos);
}

TEST_CASE("reflex::cli: an empty element of argv is skipped rather than parsed")
{
  SUBCASE("around an option")
  {
    collected_options.clear();
    const auto [rc, err] = run_numbers({"numbers"sv, ""sv, "-n"sv, "8"sv, ""sv});
    CHECK_EQ(rc, 0);
    CHECK(err.empty());
    CHECK_EQ(collected_options, std::vector{8});
  }
  SUBCASE("around a trailing argument")
  {
    collected_arguments.clear();
    const auto [rc, err] = run_tally({"tally"sv, ""sv, "6"sv, ""sv, "7"sv});
    CHECK_EQ(rc, 0);
    CHECK(err.empty());
    CHECK_EQ(collected_arguments, std::vector{6, 7});
  }
}
