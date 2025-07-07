#include <reflex/cli.hpp>

#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <pipe_capture.hpp>

using namespace reflex;
using namespace reflex::cli;

struct[[= specs<"CLI calculator.">]] //
    calculator : command
{
  [[= specs<"-v/--verbose", "Show more details.">, = _count]] //
      int verbose = false;

  [[= specs<"Add `LHS` to `RHS`.">]]                                             //
      [[= specs<":lhs:", "The left value.">]]                                    //
      [[= specs<":rhs:", "The right value.">]]                                   //
      [[= specs<":repeat:", "-r/--repeat", "Repeat output N times.">, = _count]] //
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
    [[= specs<"Left hand side value.">]] //
        float lhs;

    [[= specs<"Right hand side value.">]] //
        float rhs;
  };

  [[= specs<"Substract `RHS` from `LHS`.">]] struct _sub : base_subcommand
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

  [[= specs<"Multiplies `LHS` with `RHS`.">]] struct _mult : base_subcommand
  {
    int operator()() const noexcept
    {
      std::println("{}", lhs * rhs);
      return 0;
    }

  } mult;

  [[= specs<"Divides `LHS` by `RHS`.">]] struct _div : base_subcommand
  {
    int operator()() const noexcept
    {
      std::println("{}", lhs / rhs);
      return 0;
    }

  } div;
};

#define arraysize(__a__) (sizeof(__a__) / sizeof(__a__[0]))

#define DUMP_STR_OF(__I__) std::println(#__I__ ": {}", display_string_of(__I__))

TEST_CASE("base-test")
{
  // SECTION("option spec")
  // {
  //   static constexpr auto s = specs<"-h/--help", "Show this message and exit.">;
  //   STATIC_REQUIRE(s.is_opt);
  //   STATIC_REQUIRE(not s.is_field);
  //   STATIC_REQUIRE(s.short_switch == "-h");
  //   STATIC_REQUIRE(s.long_switch == "--help");
  //   STATIC_REQUIRE(s.help == "Show this message and exit.");
  // }
  // SECTION("non-option spec")
  // {
  //   static constexpr auto s = specs<"Help message.">;
  //   STATIC_REQUIRE(not s.is_opt);
  //   STATIC_REQUIRE(s.short_switch.empty());
  //   STATIC_REQUIRE(s.long_switch.empty());
  //   STATIC_REQUIRE(s.help == "Help message.");
  // }
  // SECTION("field option spec")
  // {
  //   static constexpr auto s = specs<":verbose:", "-v/--verbose", "Verbosity level.">;
  //   STATIC_REQUIRE(s.is_opt);
  //   STATIC_REQUIRE(s.is_field);
  //   STATIC_REQUIRE(s.field == "verbose");
  //   STATIC_REQUIRE(s.short_switch == "-v");
  //   STATIC_REQUIRE(s.long_switch == "--verbose");
  //   STATIC_REQUIRE(s.help == "Verbosity level.");
  // }
  // SECTION("non-option spec")
  // {
  //   static constexpr auto s = specs<":value:", "A value.">;
  //   STATIC_REQUIRE(not s.is_opt);
  //   STATIC_REQUIRE(s.is_field);
  //   STATIC_REQUIRE(s.field == "value");
  //   STATIC_REQUIRE(s.short_switch.empty());
  //   STATIC_REQUIRE(s.long_switch.empty());
  //   STATIC_REQUIRE(s.help == "A value.");
  // }

  calculator c{};

  // SECTION("experiments 1")
  // {
  //   constexpr auto I = ^^calculator;

  //   constexpr auto Expected = ^^detail::specs;

  //   constexpr auto members = define_static_array(
  //       members_of(I, meta::access_context::current()) |
  //       std::views::filter([](auto M)
  //                          { return is_nonstatic_data_member(M) or (is_user_declared(M) and is_function(M)); }));
  //   template for(constexpr auto M : members)
  //   {
  //     std::println("{}", display_string_of(M));

  //     constexpr auto specs_annotations = define_static_array(meta::template_annotations_of<M, ^^detail::specs>());
  //     template for(constexpr auto S : specs_annotations)
  //     {
  //       std::println("-- {}", display_string_of(S));
  //     }
  //   }
  // }
  // SECTION("getting flags")
  // {
  //   {
  //     constexpr auto flags = detail::flags_of<^^calculator, ^^calculator::verbose>();
  //     template for(constexpr auto flg : flags)
  //     {
  //       println("{}", display_string_of(flg));
  //     }
  //     STATIC_REQUIRE(detail::has_flag<^^calculator, ^^calculator::verbose, ^^_count>);
  //   }
  //   {
  //     constexpr auto add_params = define_static_array(parameters_of(^^calculator::add));
  //     constexpr auto flags      = detail::flags_of<^^calculator::add, add_params[2]>();
  //     template for(constexpr auto flg : flags)
  //     {
  //       println("{}", display_string_of(flg));
  //     }
  //     STATIC_REQUIRE(detail::has_flag<^^calculator::add, add_params[2], ^^_count>);
  //   }
  // }

  // SECTION("help")
  // {
  //   const char* args[] = {"test1", "-h"};
  //   REQUIRE(c.run(arraysize(args), args) == 0);
  // }
  // SECTION("add help")
  // {
  //   const char* args[] = {"test1", "add", "-h"};
  //   REQUIRE(c.run(arraysize(args), args) == 0);
  // }
  // SECTION("sub help")
  // {
  //   const char* args[] = {"test1", "sub", "-h"};
  //   REQUIRE(c.run(arraysize(args), args) == 0);
  // }
  SECTION("add")
  {
    const char* args[] = {"test1", "add", "2", "3"};
    int         rc     = -1;
    auto [out, err]    = capture_out_err([&] { rc = c.run(arraysize(args), args); });
    std::println("stdout: {}", out);
    std::println("stderr: {}", err);
    REQUIRE(rc == 0);
    REQUIRE(err.empty());
    REQUIRE(out == "5\n");
  }
  SECTION("add --repeat")
  {
    const char* args[] = {"test1", "add", "--repeat", "2", "3"};
    int         rc     = -1;
    auto [out, err]    = capture_out_err([&] {//
      rc = c.run(arraysize(args), args);
    });
    std::println("stdout: {}", out);
    std::println("stderr: {}", err);
    REQUIRE(rc == 0);
    REQUIRE(err.empty());
    REQUIRE(out == "5\n5\n");
  }
  SECTION("add -rr")
  {
    const char* args[] = {"test1", "add", "-rr", "2", "3"};
    int         rc     = -1;
    auto [out, err]    = capture_out_err([&] {//
      rc = c.run(arraysize(args), args);
    });
    std::println("stdout: {}", out);
    std::println("stderr: {}", err);
    REQUIRE(rc == 0);
    REQUIRE(err.empty());
    REQUIRE(out == "5\n5\n5\n");
  }
  SECTION("sub")
  {
    const char* args[] = {"test1", "-v", "sub", "2", "3"};
    int         rc     = -1;
    auto [out, err]    = capture_out_err([&] {//
      rc = c.run(arraysize(args), args);
    });
    std::println("stdout: {}", out);
    std::println("stderr: {}", err);
    REQUIRE(rc == 0);
    REQUIRE(err.empty());
    REQUIRE(out == "2 - 3 = -1\n");
  }
  SECTION("mult")
  {
    const char* args[] = {"test1", "mult", "2", "3"};
    int         rc     = -1;
    auto [out, err]    = capture_out_err([&] { rc = c.run(arraysize(args), args); });
    std::println("stdout: {}", out);
    std::println("stderr: {}", err);
    REQUIRE(rc == 0);
    REQUIRE(err.empty());
    REQUIRE(out == "6\n");
  }
  SECTION("div")
  {
    const char* args[] = {"test1", "div", "6", "3"};
    int         rc     = -1;
    auto [out, err]    = capture_out_err([&] { rc = c.run(arraysize(args), args); });
    std::println("stdout: {}", out);
    std::println("stderr: {}", err);
    REQUIRE(rc == 0);
    REQUIRE(err.empty());
    REQUIRE(out == "2\n");
  }
}
