#include <reflex/cli.hpp>

#include <iostream>
#include <pipe_capture.hpp>

using namespace reflex;
using namespace reflex::cli;

struct[[= specs{"CLI calculator."}]] //
    calculator : command
{
  [[= specs{"-v/--verbose", "Show more details."}, = _count]] //
      int verbose = false;

  [[= specs{"Add `LHS` to `RHS`."}]]                                             //
      [[= specs{":lhs:", "The left value."}]]                                    //
      [[= specs{":rhs:", "The right value."}]]                                   //
      [[= specs{":repeat:", "-r/--repeat", "Repeat output N times."}, = _count]] //
      int add(float lhs, float rhs, std::optional<int> repeat) const noexcept
  {
    for(int ii = 0; ii < repeat.value_or(0) + 1; ++ii)
    {
      std::println("{}", lhs + rhs);
    }
    return 0;
  }

  struct base_subcommand : command
  {
    [[= specs{"Left hand side value."}]] //
        float lhs;

    [[= specs{"Right hand side value."}]] //
        float rhs;
  };

  [[= specs{"Substract `RHS` from `LHS`."}]] struct _sub : base_subcommand
  {
    int operator()(this auto const& self) noexcept
    {
      if(self.parent().verbose > 0)
      {
        std::print("{} - {} = ", self.lhs, self.rhs);
      }
      std::println("{}", self.lhs - self.rhs);
      return 0;
    }
  } sub;

  [[= specs{"Multiplies `LHS` with `RHS`."}]] struct _mult : base_subcommand
  {
    int operator()() const noexcept
    {
      std::println("{}", lhs * rhs);
      return 0;
    }

  } mult;

  [[= specs{"Divides `LHS` by `RHS`."}]] struct _div : base_subcommand
  {
    int operator()() const noexcept
    {
      std::println("{}", lhs / rhs);
      return 0;
    }

  } div;
};

#include <reflex/testing_main.hpp>

using namespace std::string_view_literals;

namespace cli_calculator_tests
{
struct base_tests
{
  calculator c{};

  using output_check_fn = std::function<void(std::string_view, std::string_view)>;
  struct fixture_type
  {
    std::vector<std::string_view> args;
    int                           result       = 0;
    output_check_fn               output_check = nullptr;
  };
  static constexpr auto            prog_name      = "test";
  static const inline fixture_type help_fixture[] = {
      {.args = {prog_name, "-h"}, .result = 0, .output_check = nullptr},
      {.args = {prog_name, "add", "-h"}, .result = 0, .output_check = nullptr},
      {.args = {prog_name, "sub", "-h"}, .result = 0, .output_check = nullptr},
      {.args = {prog_name, "mult", "-h"}, .result = 0, .output_check = nullptr},
      {.args = {prog_name, "div", "-h"}, .result = 0, .output_check = nullptr},
      {.args         = {prog_name, "undefined"},
       .result       = 1,
       .output_check = [](auto out, auto err)
       { CHECK_THAT_LAZY(err.contains("unexpected argument: undefined")); }},

      {.args   = {prog_name, "add", "2", "3"},
       .result = 0,
       .output_check =
           [](auto out, auto err)
       {
         CHECK_THAT(err).is_empty();
         CHECK_THAT(out) == "5\n";
       }},
      {.args   = {prog_name, "add", "--repeat", "2", "3"},
       .result = 0,
       .output_check =
           [](auto out, auto err)
       {
         CHECK_THAT(err).is_empty();
         CHECK_THAT(out) == "5\n5\n";
       }},
      {.args   = {prog_name, "add", "-rr", "2", "3"},
       .result = 0,
       .output_check =
           [](auto out, auto err)
       {
         CHECK_THAT(err).is_empty();
         CHECK_THAT(out) == "5\n5\n5\n";
       }},
      {.args   = {prog_name, "-v", "sub", "2", "3"},
       .result = 0,
       .output_check =
           [](auto out, auto err)
       {
         CHECK_THAT(err).is_empty();
         CHECK_THAT(out) == "2 - 3 = -1\n";
       }},
      {.args   = {prog_name, "mult", "2", "3"},
       .result = 0,
       .output_check =
           [](auto out, auto err)
       {
         CHECK_THAT(err).is_empty();
         CHECK_THAT(out) == "6\n";
       }},
      {.args   = {prog_name, "div", "6", "3"},
       .result = 0,
       .output_check =
           [](auto out, auto err)
       {
         CHECK_THAT(err).is_empty();
         CHECK_THAT(out) == "2\n";
       }},
  };

  [[= testing::parametrize<^^help_fixture>]] void
      test(std::vector<std::string_view> const& args, int result, output_check_fn const& check)
  {
    const char* aa[args.size()];
    int         ii = 0;
    for(auto const& a : args)
    {
      aa[ii++] = a.data();
    }
    int rc          = -1;
    std::string out, err;
    std::tie(out, err) = capture_out_err([&] { rc = c.run(args.size(), aa); });
    CHECK_CONTEXT(out, err);
    CHECK_THAT(rc) == result;
    if(check)
    {
      check(out, err);
    }
  }
};
} // namespace cli_calculator_tests
