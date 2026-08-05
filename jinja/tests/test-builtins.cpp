#include <doctest/doctest.h>

import reflex.jinja;
import std;

using namespace reflex;
using namespace reflex::jinja;
using namespace std::string_literals;

using basic_context = expr::context<>;
using value         = basic_context::value_type;
using object        = basic_context::object_type;
using array         = basic_context::array_type;

TEST_CASE("reflex::jinja: builtins")
{
  basic_context ctx;

  SUBCASE("length of a string")
  {
    CHECK(std::get<int>(expr::evaluate(R"(length("abc"))", ctx)) == 3);
    CHECK(std::get<int>(expr::evaluate(R"(length(""))", ctx)) == 0);
  }

  SUBCASE("length of an array")
  {
    ctx.set("xs", array{1, 2, 3});
    CHECK(std::get<int>(expr::evaluate("length(xs)", ctx)) == 3);

    ctx.set("empty", array{});
    CHECK(std::get<int>(expr::evaluate("length(empty)", ctx)) == 0);
  }

  SUBCASE("length of an array bound by reference")
  {
    // A named array binds as a pointer alternative, not by value.
    array xs{1, 2, 3, 4};
    ctx.set("xs", xs);
    CHECK(std::get<int>(expr::evaluate("length(xs)", ctx)) == 4);
  }

  SUBCASE("length of an object")
  {
    ctx.set(
        "o", {
                 {"a", 1},
                 {"b", 2}
    });
    CHECK(std::get<int>(expr::evaluate("length(o)", ctx)) == 2);
  }

  SUBCASE("length as a pipe")
  {
    CHECK(std::get<int>(expr::evaluate(R"("hello" | length)", ctx)) == 5);
  }

  SUBCASE("length rejects a wrong arity")
  {
    CHECK_THROWS_AS(expr::evaluate("length()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(length("a", "b"))", ctx), std::runtime_error);
    CHECK_THROWS_WITH(expr::evaluate("length()", ctx), doctest::Contains("length"));
  }

  SUBCASE("length rejects a wrong type")
  {
    CHECK_THROWS_AS(expr::evaluate("length(42)", ctx), std::runtime_error);
    CHECK_THROWS_WITH(expr::evaluate("length(42)", ctx), doctest::Contains("length"));
  }

  SUBCASE("a user def wins over a builtin")
  {
    ctx.def("length", [](std::span<const value>) -> value { return -1; });
    CHECK(std::get<int>(expr::evaluate(R"(length("abc"))", ctx)) == -1);
  }

  SUBCASE("an unknown name names itself")
  {
    CHECK_THROWS_AS(expr::evaluate("missing_filter(1)", ctx), std::runtime_error);
    CHECK_THROWS_WITH(expr::evaluate("missing_filter(1)", ctx), doctest::Contains("missing_filter"));
    CHECK_THROWS_WITH(
        expr::evaluate(R"("x" | missing_filter)", ctx), doctest::Contains("missing_filter"));
  }

  SUBCASE("render end to end")
  {
    ctx.set("name", "world"s);
    CHECK(jinja::render(jinja::parse("{{ name | length }}"), ctx) == "5");
  }
}
