#include <doctest/doctest.h>

import reflex.jinja;
import std;

using namespace reflex;
using namespace reflex::jinja;
using namespace std::string_literals;

#define JINJA(...) #__VA_ARGS__

using basic_context = expr::context<>;
using value         = basic_context::value_type;
using object        = basic_context::object_type;
using array         = basic_context::array_type;

TEST_CASE("reflex::jinja: expr")
{
  SUBCASE("literals")
  {
    auto result = expr::evaluate(JINJA(42));
    CHECK(std::get<int>(result) == 42);

    result = expr::evaluate(JINJA(3.14));
    CHECK(std::get<double>(result) == doctest::Approx(3.14));

    result = expr::evaluate(JINJA("hello"));
    CHECK(std::get<std::string>(result) == "hello");

    result = expr::evaluate(JINJA(true));
    CHECK(std::get<bool>(result) == true);

    result = expr::evaluate(JINJA(false));
    CHECK(std::get<bool>(result) == false);

    result = expr::evaluate(JINJA(null));
    CHECK(result.is_null());
  }

  SUBCASE("arithmetic")
  {
    CHECK(std::get<int>(expr::evaluate(JINJA(1 + 2))) == 3);
    CHECK(std::get<int>(expr::evaluate(JINJA(10 - 4))) == 6);
    CHECK(std::get<int>(expr::evaluate(JINJA(3 * 4))) == 12);
    CHECK(std::get<int>(expr::evaluate(JINJA(10 / 2))) == 5);
    CHECK(std::get<int>(expr::evaluate(JINJA(10 % 3))) == 1);

    CHECK(std::get<double>(expr::evaluate(JINJA(1.5 + 2.5))) == doctest::Approx(4.0));
    CHECK(std::get<double>(expr::evaluate(JINJA(1.5 * 2.0))) == doctest::Approx(3.0));

    // mixed int/double promotes to double
    CHECK(std::get<double>(expr::evaluate(JINJA(1 + 2.0))) == doctest::Approx(3.0));

    // string concatenation
    CHECK(std::get<std::string>(expr::evaluate(JINJA("foo" + "bar"))) == "foobar");

    // unary minus
    CHECK(std::get<int>(expr::evaluate(JINJA(-7))) == -7);
    CHECK(std::get<double>(expr::evaluate(JINJA(-1.5))) == doctest::Approx(-1.5));

    // parentheses / precedence
    CHECK(std::get<int>(expr::evaluate(JINJA(2 + 3 * 4))) == 14);
    CHECK(std::get<int>(expr::evaluate(JINJA((2 + 3) * 4))) == 20);
  }

  SUBCASE("comparison")
  {
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 == 1))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 == 2))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 != 2))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 != 1))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 < 2))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(2 < 1))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 <= 1))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(2 <= 1))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(2 > 1))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 > 2))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 >= 1))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 >= 2))) == false);

    // mixed int/double
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 == 1.0))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 < 1.5))) == true);

    // string equality
    CHECK(std::get<bool>(expr::evaluate(JINJA("a" == "a"))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA("a" == "b"))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA("a" != "b"))) == true);
  }

  SUBCASE("logical")
  {
    CHECK(std::get<bool>(expr::evaluate(JINJA(true and true))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(true and false))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(false and true))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(false or true))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(false or false))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(not true))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(not false))) == true);

    // operator aliases
    CHECK(std::get<bool>(expr::evaluate(JINJA(true && false))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(false || true))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(!true))) == false);

    // compound
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 == 1 and 2 == 2))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 == 1 and 2 == 3))) == false);
    CHECK(std::get<bool>(expr::evaluate(JINJA(1 == 2 or 2 == 2))) == true);
    CHECK(std::get<bool>(expr::evaluate(JINJA(not(1 == 2) and not false))) == true);
  }

  SUBCASE("variables")
  {
    expr::context ctx;
    ctx.set("x", 42).set("pi", 3.14).set("name", std::string{"world"}).set("flag", true);

    CHECK(std::get<int>(expr::evaluate("x", ctx)) == 42);
    CHECK(std::get<double>(expr::evaluate("pi", ctx)) == doctest::Approx(3.14));
    CHECK(std::get<std::string>(expr::evaluate("name", ctx)) == "world");
    CHECK(std::get<bool>(expr::evaluate("flag", ctx)) == true);

    // unknown variable -> null
    CHECK(expr::evaluate("unknown", ctx).is_null());

    // variable in expression
    CHECK(std::get<bool>(expr::evaluate("x == 42", ctx)) == true);
    CHECK(std::get<bool>(expr::evaluate("x >= 1", ctx)) == true);
    CHECK(std::get<bool>(expr::evaluate("x < 0", ctx)) == false);
    CHECK(std::get<bool>(expr::evaluate("x == 0", ctx)) == false);
  }

  SUBCASE("dotted member access")
  {
    basic_context ctx;
    ctx.set(
        "user", {
                    {"name", "alice"s},
                    {"age",  30      }
    });

    CHECK(std::get<std::string>(expr::evaluate("user.name", ctx)) == "alice");
    CHECK(std::get<int>(expr::evaluate("user.age", ctx)) == 30);
    CHECK(std::get<bool>(expr::evaluate("user.age >= 18", ctx)) == true);
  }

  SUBCASE("function calls")
  {
    using context = expr::context<>;
    context ctx;
    ctx.def("abs", [](auto&& args) -> context::value_type {
      if(args.size() != 1)
        throw std::runtime_error("abs() takes 1 argument");
      if(auto* i = std::get_if<int>(&args[0]))
        return int{*i < 0 ? -*i : *i};
      if(auto* d = std::get_if<double>(&args[0]))
        return std::abs(*d);
      throw std::runtime_error("abs() requires a numeric argument");
    });

    ctx.def("max", [](auto&& args) -> context::value_type {
      if(args.size() != 2)
        throw std::runtime_error("max() takes 2 arguments");
      if(auto* la = std::get_if<int>(&args[0]))
        if(auto* rb = std::get_if<int>(&args[1]))
          return int{*la > *rb ? *la : *rb};
      throw std::runtime_error("max() requires integer arguments");
    });

    ctx.def("concat", [](auto&& args) -> context::value_type {
      std::string result;
      for(const auto& a : args)
        if(auto* s = std::get_if<std::string>(&a))
          result += *s;
      return result;
    });

    CHECK(std::get<int>(expr::evaluate("abs(-7)", ctx)) == 7);
    CHECK(std::get<int>(expr::evaluate("abs(7)", ctx)) == 7);
    CHECK(std::get<int>(expr::evaluate("max(3, 5)", ctx)) == 5);
    CHECK(std::get<int>(expr::evaluate("max(9, 2)", ctx)) == 9);
    CHECK(std::get<std::string>(expr::evaluate(R"(concat("a","b","c"))", ctx)) == "abc");

    // call result used in expression
    CHECK(std::get<bool>(expr::evaluate("abs(-3) < 5", ctx)) == true);
    CHECK(std::get<bool>(expr::evaluate("max(1, 2) == 2", ctx)) == true);
  }

  SUBCASE("evaluate_bool")
  {
    expr::context ctx;
    ctx.set("x", 1).set("y", 0).set("s", "hello"s).set("empty", ""s);

    CHECK(expr::evaluate_bool("x", ctx) == true);
    CHECK(expr::evaluate_bool("y", ctx) == false);
    CHECK(expr::evaluate_bool("s", ctx) == true);
    CHECK(expr::evaluate_bool("empty", ctx) == false);
    CHECK(expr::evaluate_bool("x == 1", ctx) == true);
    CHECK(expr::evaluate_bool("x == 1 and s", ctx) == true);
    CHECK(expr::evaluate_bool("y or x", ctx) == true);
    CHECK(expr::evaluate_bool("not x", ctx) == false);
  }
}

struct aggregate1
{
  int         a;
  std::string b;
};

using namespace reflex::literals;

TEST_CASE("reflex::jinja::expr: aggregates")
{
  using namespace expr;
  aggregate1    agg{1, "hello"};
  expr::context ctx{"a"_na = agg};
  ctx.set("x", 1).set("y", 0);
  CHECK(expr::evaluate_bool("a.a == 1", ctx) == true);
  CHECK(expr::evaluate_bool("a.b == \"hello\"", ctx) == true);
  CHECK(expr::evaluate_bool("a.a == x", ctx) == true);
  CHECK(expr::evaluate_bool("a.a != y", ctx) == true);
}