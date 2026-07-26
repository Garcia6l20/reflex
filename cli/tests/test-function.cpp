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
    for(auto __ : std::views::iota(0, count))
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
    for(auto __ : std::views::iota(0, count))
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
  return value.value_or(-1);
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
