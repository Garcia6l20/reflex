#include <doctest/doctest.h>

#include <reflex/cli.hpp>
#include <testutils.hpp>

using namespace reflex;

[[= cli::command{"Print a line of dots."}]]
int dots(
    [[= cli::argument{"How many dots."}]] int                count,
    [[= cli::option{"-r/--repeat", "Repeat the line."}]] int repeat)
{
  for(auto _ : std::views::iota(0, std::max(repeat, 1)))
  {
    for(auto _ : std::views::iota(0, count))
    {
      std::print(".");
    }
    std::println();
  }
  return 0;
}

// Same parameter names and types as dots: two commands whose members are
// indistinguishable are what a per-function aggregate has to keep apart.
int dashes(
    [[= cli::argument{"How many dashes."}]] int                count,
    [[= cli::option{"-r/--repeat", "Repeat the line."}]] int   repeat)
{
  for(auto _ : std::views::iota(0, std::max(repeat, 1)))
  {
    for(auto _ : std::views::iota(0, count))
    {
      std::print("-");
    }
    std::println();
  }
  return 0;
}

void nothing()
{}

int maybe([[= cli::argument{"Optional value."}]] std::optional<int> value)
{
  std::println("{}", value.value_or(-1));
  return 0;
}

int only_options([[= cli::option{"-n/--name", "A name."}]] std::string name)
{
  return int(name.size());
}

TEST_CASE("reflex::cli: a function's parameters describe a command")
{
  static constexpr auto raw = cli::detail::raw_parse<^^cli::detail::command_args<^^dots>>();
  // one argument, and the declared option on top of the three built-in ones
  static_assert(std::get<0>(raw).size() == 1);
  static_assert(std::get<1>(raw).size() == 4);
  static_assert(std::get<2>(raw).empty());

  static constexpr auto arg = cli::detail::argument_info{std::get<0>(raw)[0]};
  static_assert(arg.name() == "count");
  static_assert(*arg.help() == "How many dots.");

  static constexpr auto opt = cli::detail::option_info{std::get<1>(raw)[3]};
  static_assert(opt.name() == "repeat");
  static_assert(*opt.help() == "Repeat the line.");
  static_assert(*opt.switches.s == "-r");
  static_assert(*opt.switches.l == "--repeat");

  // the function's own annotation describes the aggregate, so usage can print it
  static constexpr auto described =
      cli::detail::command_annotation_for(^^cli::detail::command_args<^^dots>);
  static_assert(*described.help == "Print a line of dots.");
}

TEST_CASE("reflex::cli: two commands with the same parameters stay apart")
{
  static constexpr auto lhs = cli::detail::raw_parse<^^cli::detail::command_args<^^dots>>();
  static constexpr auto rhs = cli::detail::raw_parse<^^cli::detail::command_args<^^dashes>>();

  static_assert(std::get<0>(lhs)[0] != std::get<0>(rhs)[0]);
  static_assert(*cli::detail::argument_info{std::get<0>(lhs)[0]}.help() == "How many dots.");
  static_assert(*cli::detail::argument_info{std::get<0>(rhs)[0]}.help() == "How many dashes.");
}

TEST_CASE("reflex::cli: a function without parameters describes an empty command")
{
  static constexpr auto raw = cli::detail::raw_parse<^^cli::detail::command_args<^^nothing>>();
  static_assert(std::get<0>(raw).empty());
  static_assert(std::get<1>(raw).size() == 3);
  static_assert(std::get<2>(raw).empty());
}

TEST_CASE("reflex::cli: an optional parameter stays optional")
{
  static constexpr auto raw = cli::detail::raw_parse<^^cli::detail::command_args<^^maybe>>();
  static_assert(std::get<0>(raw).size() == 1);
  static_assert(meta::is_template_instance_of(
      cli::detail::argument_info{std::get<0>(raw)[0]}.type(), ^^std::optional));
}

TEST_CASE("reflex::cli: a function may declare options only")
{
  static constexpr auto raw = cli::detail::raw_parse<^^cli::detail::command_args<^^only_options>>();
  static_assert(std::get<0>(raw).empty());
  static_assert(std::get<1>(raw).size() == 4);
}

TEST_CASE("reflex::cli: a synthesized member is a real member")
{
  cli::detail::command_args<^^dots> args{};
  args.count  = 3;
  args.repeat = 2;

  const auto [out, err] = testutils::capture_out_err([&] {
    CHECK_EQ(std::apply([](auto&&... xs) { return dots(xs...); }, reflex::to_tuple(args)), 0);
  });
  CHECK(err.empty());
  CHECK_EQ(out, "...\n...\n");
}

using namespace std::string_view_literals;

TEST_CASE("reflex::cli: a function runs as a command")
{
  SUBCASE("an argument alone")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^dots>({"dots"sv, "3"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "...\n");
  }

  SUBCASE("an option in long form")
  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(cli::run<^^dots>({"dots"sv, "2"sv, "--repeat"sv, "2"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "..\n..\n");
  }

  SUBCASE("an option in short form")
  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(cli::run<^^dots>({"dots"sv, "2"sv, "-r"sv, "3"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "..\n..\n..\n");
  }

  SUBCASE("a function returning void reports success")
  {
    CHECK_EQ(cli::run<^^nothing>({"nothing"sv}), 0);
  }

  SUBCASE("a function without parameters takes none")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_NE(cli::run<^^nothing>({"nothing"sv, "3"sv}), 0); });
    CHECK_FALSE(err.empty());
  }
}

TEST_CASE("reflex::cli: two function commands do not collide")
{
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^dots>({"dots"sv, "3"sv}), 0); });
    CHECK_EQ(out, "...\n");
  }
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^dashes>({"dashes"sv, "3"sv}), 0); });
    CHECK_EQ(out, "---\n");
  }
}

TEST_CASE("reflex::cli: a function command reports what it cannot parse")
{
  SUBCASE("an unknown option")
  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_NE(cli::run<^^dots>({"dots"sv, "3"sv, "--nope"sv}), 0); });
    CHECK_FALSE(err.empty());
  }

  SUBCASE("a missing argument")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_NE(cli::run<^^dots>({"dots"sv}), 0); });
    CHECK_FALSE(err.empty());
  }

  SUBCASE("a value the number does not consume whole")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_NE(cli::run<^^dots>({"dots"sv, "2abc"sv}), 0); });
    CHECK_NE(err.find("invalid argument value: 2abc"), std::string::npos);
  }
}

TEST_CASE("reflex::cli: an optional parameter may be left out")
{
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^maybe>({"maybe"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "-1\n");
  }
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^maybe>({"maybe"sv, "7"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "7\n");
  }
}

// A command holding nothing but sub-commands has no operator() either, and is
// meant to report at run time rather than fail to compile. Supplying the invoke
// from outside the command is what could have turned that into a build error.
struct[[= cli::command{"A leaf."}]] leaf
{
  int operator()() const
  {
    return 0;
  }
};

struct[[= cli::command{"Holds a sub-command and nothing else."}]] branch
{
  leaf sub;
};

struct[[= cli::command{"Print a line of pluses."}]] pluses
{
  [[= cli::argument{"How many pluses."}]] int count;

  int operator()() const
  {
    for(auto _ : std::views::iota(0, count))
    {
      std::print("+");
    }
    std::println();
    return 0;
  }
};

TEST_CASE("reflex::cli: a struct command and a function command share a unit")
{
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run(pluses{}, {"pluses"sv, "3"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "+++\n");
  }
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^dots>({"dots"sv, "3"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "...\n");
  }
}

TEST_CASE("reflex::cli: a command with no way to run reports it")
{
  const auto [out, err] =
      testutils::capture_out_err([] { CHECK_NE(cli::run(branch{}, {"branch"sv}), 0); });
  CHECK_NE(err.find("no command to execute"), std::string::npos);
}

[[= cli::command{"Print a line of stars."}]]
constexpr auto stars = []([[= cli::argument{"How many stars."}]] int count) {
  for(auto _ : std::views::iota(0, count))
  {
    std::print("*");
  }
  std::println();
  return 0;
};

// The state a capturing lambda holds lives in the variable the user names, and
// the invoke splices that variable, so it reaches the call unchanged.
int hidden = 4;
auto scaled = [factor = 3]([[= cli::argument{"A number."}]] int n) {
  std::println("{}", n * factor + hidden);
  return 0;
};

struct[[= cli::command{"Print a line of hashes."}]] hashes
{
  int operator()([[= cli::argument{"How many hashes."}]] int count) const
  {
    for(auto _ : std::views::iota(0, count))
    {
      std::print("#");
    }
    std::println();
    return 0;
  }
};

TEST_CASE("reflex::cli: a callable runs as a command")
{
  SUBCASE("a lambda")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^stars>({"stars"sv, "3"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "***\n");
  }

  SUBCASE("a capturing lambda keeps its captures")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^scaled>({"scaled"sv, "5"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "19\n");
  }

  SUBCASE("a function object type")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^hashes>({"hashes"sv, "4"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "####\n");
  }

  SUBCASE("a lambda's own annotation describes it")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run<^^stars>({"stars"sv, "--help"sv}), 0); });
    CHECK_NE(out.find("Print a line of stars."), std::string::npos);
    CHECK_NE(out.find("count            How many stars."), std::string::npos);
  }
}

// A member function sub-command reads its parent's options because the parent
// is bound at the call. A nested struct has to be handed a back-reference to
// get the same thing.
struct[[= cli::command{"Shape tool."}]] shapes
{
  [[= cli::option{"-w/--width", "How wide."}]] int width = 1;

  [[= cli::command{"Draw a row of dots."}]]
  int row([[= cli::argument{"Which character."}]] std::string glyph)
  {
    for(auto _ : std::views::iota(0, width))
    {
      std::print("{}", glyph);
    }
    std::println();
    return 0;
  }

  [[= cli::command{"Report the width."}]]
  void report() const
  {
    std::println("width={}", width);
  }

  // a nested struct sub-command alongside the function ones
  struct[[= cli::command{"A nested struct sub-command."}]]
  {
    [[= cli::argument{"A label."}]] std::string label = "";

    int operator()() const
    {
      std::println("nested {}", label);
      return 0;
    }
  } nested{};
};

TEST_CASE("reflex::cli: a member function is a sub-command")
{
  SUBCASE("it runs")
  {
    const auto [out, err] =
        testutils::capture_out_err([] { CHECK_EQ(cli::run(shapes{}, {"shapes"sv, "row"sv, "x"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "x\n");
  }

  SUBCASE("it reads an option the parent parsed before the descent")
  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(cli::run(shapes{}, {"shapes"sv, "-w"sv, "4"sv, "row"sv, "y"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "yyyy\n");
  }

  SUBCASE("a const sub-command returning void works")
  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(cli::run(shapes{}, {"shapes"sv, "-w"sv, "3"sv, "report"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "width=3\n");
  }

  SUBCASE("a struct sub-command and a function sub-command coexist")
  {
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_EQ(cli::run(shapes{}, {"shapes"sv, "nested"sv, "here"sv}), 0); });
    CHECK(err.empty());
    CHECK_EQ(out, "nested here\n");
  }

  SUBCASE("a parent option after the sub-command name is not the parent's")
  {
    // Same rule as a nested struct sub-command: the parent's options have to
    // come first, because after the descent the parser is matching against the
    // sub-command's own list.
    const auto [out, err] = testutils::capture_out_err(
        [] { CHECK_NE(cli::run(shapes{}, {"shapes"sv, "report"sv, "-w"sv, "4"sv}), 0); });
    CHECK_NE(err.find("unknown option: -w"), std::string::npos);
  }
}

TEST_CASE("reflex::cli: a hybrid command lists both kinds of sub-command")
{
  const auto [out, err] =
      testutils::capture_out_err([] { CHECK_EQ(cli::run(shapes{}, {"shapes"sv, "--help"sv}), 0); });
  CHECK(err.empty());
  CHECK_EQ(
      out,
      "USAGE: shapes [OPTIONS...] ARGUMENTS...\n"
      "\n"
      "Shape tool.\n"
      "\n"
      "OPTIONS:\n"
      "  --help               Print this message and exit.\n"
      "  --install-completion Install shell completion.\n"
      "  --show-completion    Show shell completion.\n"
      "  -w/--width           How wide.\n"
      "\n"
      "COMMANDS:\n"
      "  row              Draw a row of dots.\n"
      "  report           Report the width.\n"
      "  nested           A nested struct sub-command.\n"
      "\n");
}

TEST_CASE("reflex::cli: a member function sub-command prints its own usage")
{
  const auto [out, err] = testutils::capture_out_err(
      [] { CHECK_EQ(cli::run(shapes{}, {"shapes"sv, "row"sv, "--help"sv}), 0); });
  CHECK(err.empty());
  CHECK_NE(out.find("Draw a row of dots."), std::string::npos);
  CHECK_NE(out.find("glyph            Which character."), std::string::npos);
}

TEST_CASE("reflex::cli: a function command prints its usage")
{
  const auto [out, err] =
      testutils::capture_out_err([] { CHECK_EQ(cli::run<^^dots>({"dots"sv, "--help"sv}), 0); });
  CHECK(err.empty());
  CHECK_EQ(
      out,
      "USAGE: dots [OPTIONS...] ARGUMENTS...\n"
      "\n"
      "Print a line of dots.\n"
      "\n"
      "OPTIONS:\n"
      "  --help               Print this message and exit.\n"
      "  --install-completion Install shell completion.\n"
      "  --show-completion    Show shell completion.\n"
      "  -r/--repeat          Repeat the line.\n"
      "\n"
      "ARGUMENTS:\n"
      "  count            How many dots.\n"
      "\n"
      "\n");
}
