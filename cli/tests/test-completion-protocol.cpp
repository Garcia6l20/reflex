#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace reflex;
using namespace testutils;
using namespace std::string_view_literals;

namespace
{
struct[[= cli::command{"Every builtin completer, one per sub-command."}]] completers_cli
{
  struct[[= cli::command{"Any path."}]]
  {
    [[= cli::argument{"Any path."}, = cli::completers::path{}]] //
    std::string target = "";

    int operator()() const
    {
      return 0;
    }
  } any{};

  struct[[= cli::command{"A directory."}]]
  {
    [[= cli::argument{"A directory."}, = cli::completers::path<>::dirs{}]] //
    std::string target = "";

    int operator()() const
    {
      return 0;
    }
  } dir{};

  struct[[= cli::command{"A file."}]]
  {
    [[= cli::argument{"A file."}, = cli::completers::path<>::files{}]] //
    std::string target = "";

    int operator()() const
    {
      return 0;
    }
  } file{};

  struct[[= cli::command{"A JSON file."}]]
  {
    [[= cli::argument{"A JSON file."}, = cli::completers::path{"*.json"}]] //
    std::string target = "";

    int operator()() const
    {
      return 0;
    }
  } json{};

  struct[[= cli::command{"An argument nothing can complete."}]]
  {
    [[= cli::argument{"A name."}]] //
    std::string name = "";

    int operator()() const
    {
      return 0;
    }
  } bare{};

  struct[[= cli::command{"A completer that leaves the word open."}]]
  {
    static auto partial_completer(std::string_view)
    {
      return std::make_tuple(
          false, std::array{
                     cli::completion<>{
                                       .type        = cli::completion_type::plain,
                                       .value       = "prefix",
                                       .description = "One of several"}
      });
    }

    [[= cli::argument{"A word."}, = cli::complete{^^partial_completer}]] //
    std::string word = "";

    [[= cli::option{"-w/--width", "A number."}]] //
    int width = 0;

    int operator()() const
    {
      return 0;
    }
  } partial{};

  int operator()() const
  {
    return 0;
  }
};

[[= cli::command{"A function command that completes."}]]
int typed([[= cli::argument{"A path."}, = cli::completers::path{"*.md"}]] std::string path)
{
  return int(path.size());
}

std::vector<std::string> lines_of(std::string_view out)
{
  auto                     ss = std::istringstream{std::string{out}};
  std::vector<std::string> lines;
  for(std::string line; std::getline(ss, line);)
  {
    lines.push_back(line);
  }
  return lines;
}

struct completion_env
{
  completion_env(std::optional<std::string_view> line, std::optional<std::string_view> point)
  {
    set_env("_REFLEX_COMPLETE", "zsh_complete", true);
    if(line)
    {
      set_env("_REFLEX_COMP_LINE", std::string{*line}.c_str(), true);
    }
    else
    {
      unset_env("_REFLEX_COMP_LINE");
    }
    if(point)
    {
      set_env("_REFLEX_COMP_POINT", std::string{*point}.c_str(), true);
    }
    else
    {
      unset_env("_REFLEX_COMP_POINT");
    }
  }

  ~completion_env()
  {
    unset_env("_REFLEX_COMPLETE");
    unset_env("_REFLEX_COMP_LINE");
    unset_env("_REFLEX_COMP_POINT");
  }
};

auto complete_line(std::optional<std::string_view> line, std::optional<std::string_view> point)
{
  const auto guard = completion_env{line, point};

  int  rc = -1;
  auto out =
      capture_out_err([&] { rc = cli::run(completers_cli{}, std::initializer_list{"cli"sv}); })
          .first;
  return std::pair{rc, lines_of(out)};
}
} // namespace

TEST_CASE("reflex::cli: the completion protocol refuses a request it cannot read")
{
  SUBCASE("no command line to complete")
  {
    const auto [rc, lines] = complete_line({}, "3");
    CHECK_EQ(rc, 1);
    CHECK(lines.empty());
  }
  SUBCASE("an empty command line to complete")
  {
    const auto [rc, lines] = complete_line("", "3");
    CHECK_EQ(rc, 1);
    CHECK(lines.empty());
  }
  SUBCASE("a completion point that is not a number")
  {
    const auto [rc, lines] = complete_line("cli ", "here");
    CHECK_EQ(rc, 1);
    CHECK(lines.empty());
  }
  SUBCASE("a completion point before the command name")
  {
    const auto [rc, lines] = complete_line("cli ", "0");
    CHECK_EQ(rc, 1);
    CHECK(lines.empty());
  }
  SUBCASE("no completion point at all is the first word")
  {
    const auto [rc, lines] = complete_line("cli ", {});
    CHECK_EQ(rc, 0);
    CHECK_FALSE(lines.empty());
  }
  SUBCASE("a command line holding nothing but the command name")
  {
    const auto [rc, lines] = complete_line("cli", "1");
    CHECK_EQ(rc, 0);
  }
  SUBCASE("a command line holding nothing but spaces")
  {
    const auto [rc, lines] = complete_line("   ", "1");
    CHECK_EQ(rc, 0);
  }
}

TEST_CASE("reflex::cli: a quoted word in the command line is one word")
{
  SUBCASE("the quotes are stripped")
  {
    const auto [rc, lines] = complete_line("cli \"any\" ", "3");
    CHECK_EQ(rc, 0);
    const auto values = completion_values(lines)
                      | std::views::transform(&cli::completion<>::value)
                      | std::ranges::to<std::vector>();
    CHECK(std::ranges::contains(values, "*"sv));
  }
  SUBCASE("a quote that is never closed completes nothing")
  {
    const auto [rc, lines] = complete_line("cli \"any", "3");
    CHECK_EQ(rc, 0);
  }
}

TEST_CASE("reflex::cli: the builtin path completers answer with one entry")
{
  const auto values_for = [](std::string_view line) {
    const auto [rc, lines] = complete_line(line, "3");
    CHECK_EQ(rc, 0);
    return completion_values(lines);
  };

  SUBCASE("any path")
  {
    const auto values = values_for("cli any ");
    REQUIRE_EQ(values.size(), 1);
    CHECK_EQ(values[0].type, cli::completion_type::file);
    CHECK_EQ(values[0].value, "*");
  }
  SUBCASE("a directory")
  {
    const auto values = values_for("cli dir ");
    REQUIRE_EQ(values.size(), 1);
    CHECK_EQ(values[0].type, cli::completion_type::dir);
    CHECK_EQ(values[0].value, "");
    CHECK_EQ(values[0].description, "Directory");
  }
  SUBCASE("a file")
  {
    const auto values = values_for("cli file ");
    REQUIRE_EQ(values.size(), 1);
    CHECK_EQ(values[0].type, cli::completion_type::file);
    CHECK_EQ(values[0].value, "*");
  }
  SUBCASE("a pattern of its own")
  {
    const auto values = values_for("cli json ");
    REQUIRE_EQ(values.size(), 1);
    CHECK_EQ(values[0].type, cli::completion_type::file);
    CHECK_EQ(values[0].value, "*.json");
  }
}

TEST_CASE("reflex::cli: an argument carrying no completer answers with nothing")
{
  const auto [rc, lines] = complete_line("cli bare ", "3");
  CHECK_EQ(rc, 0);
  CHECK(completion_values(lines).empty());
}

TEST_CASE("reflex::cli: a completer may report the word as still open")
{
  const auto [rc, lines] = complete_line("cli partial ", "3");
  CHECK_EQ(rc, 0);
  REQUIRE_FALSE(lines.empty());
  CHECK_EQ(lines[0], "0");
}

TEST_CASE("reflex::cli: a long switch typed whole is offered back")
{
  const auto [rc, lines] = complete_line("cli partial word --width", "2");
  CHECK_EQ(rc, 0);
  const auto values = completion_values(lines)
                    | std::views::transform(&cli::completion<>::value)
                    | std::ranges::to<std::vector>();
  CHECK(std::ranges::contains(values, "--width"sv));
}

TEST_CASE("reflex::cli: a function command completes like a struct command")
{
  const auto guard = completion_env{"typed "sv, "2"sv};

  int  rc = -1;
  auto out =
      capture_out_err([&] { rc = cli::run<^^typed>(std::initializer_list{"typed"sv}); }).first;
  CHECK_EQ(rc, 0);

  const auto values = completion_values(lines_of(out));
  REQUIRE_EQ(values.size(), 1);
  CHECK_EQ(values[0].value, "*.md");
}
