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

TEST_CASE("reflex::jinja: builtins tier 1")
{
  basic_context ctx;

  SUBCASE("count aliases length")
  {
    CHECK(std::get<int>(expr::evaluate(R"(count("abcd"))", ctx)) == 4);
    CHECK_THROWS_AS(expr::evaluate("count()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("count(42)", ctx), std::runtime_error);
  }

  SUBCASE("default")
  {
    CHECK(std::get<std::string>(expr::evaluate(R"(default(null, "x"))", ctx)) == "x");
    // zero is not null
    CHECK(std::get<int>(expr::evaluate(R"(default(0, "x"))", ctx)) == 0);
    CHECK(std::get<std::string>(expr::evaluate(R"("" | default("x"))", ctx)) == "");
    CHECK(std::get<std::string>(expr::evaluate(R"(missing | default("x"))", ctx)) == "x");

    CHECK_THROWS_AS(expr::evaluate("default(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("default(1, 2, 3)", ctx), std::runtime_error);
  }

  SUBCASE("join")
  {
    ctx.set("xs", array{1, 2, 3});
    CHECK(std::get<std::string>(expr::evaluate(R"(join(xs, "-"))", ctx)) == "1-2-3");
    CHECK(std::get<std::string>(expr::evaluate("join(xs)", ctx)) == "123");
    CHECK(std::get<std::string>(expr::evaluate(R"(xs | join(", "))", ctx)) == "1, 2, 3");

    ctx.set("empty", array{});
    CHECK(std::get<std::string>(expr::evaluate("join(empty)", ctx)) == "");

    ctx.set("one", array{"a"s});
    CHECK(std::get<std::string>(expr::evaluate(R"(join(one, "-"))", ctx)) == "a");

    CHECK_THROWS_AS(expr::evaluate("join()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(join("abc"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("join(xs, 1)", ctx), std::runtime_error);
  }

  SUBCASE("reverse")
  {
    CHECK(std::get<std::string>(expr::evaluate(R"(reverse("abc"))", ctx)) == "cba");
    CHECK(std::get<std::string>(expr::evaluate(R"(reverse(""))", ctx)) == "");

    ctx.set("xs", array{1, 2, 3});
    CHECK(std::get<std::string>(expr::evaluate(R"(xs | reverse | join("-"))", ctx)) == "3-2-1");

    ctx.set("empty", array{});
    CHECK(std::get<std::string>(expr::evaluate("join(reverse(empty))", ctx)) == "");

    CHECK_THROWS_AS(expr::evaluate(R"(reverse("a", "b"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("reverse(1)", ctx), std::runtime_error);
  }

  SUBCASE("range")
  {
    CHECK(std::get<std::string>(expr::evaluate(R"(join(range(3), ","))", ctx)) == "0,1,2");
    CHECK(std::get<std::string>(expr::evaluate(R"(join(range(1, 4), ","))", ctx)) == "1,2,3");
    CHECK(std::get<std::string>(expr::evaluate(R"(join(range(4, 1, -1), ","))", ctx)) == "4,3,2");
    CHECK(std::get<std::string>(expr::evaluate(R"(join(range(0, 10, 3), ","))", ctx)) == "0,3,6,9");
    CHECK(std::get<int>(expr::evaluate("length(range(0))", ctx)) == 0);
    CHECK(std::get<int>(expr::evaluate("length(range(1, 4, -1))", ctx)) == 0);

    CHECK_THROWS_AS(expr::evaluate("range()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("range(1, 2, 3, 4)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("range(1, 2, 0)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("range(2000000)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(range("3"))", ctx), std::runtime_error);
  }

  SUBCASE("format")
  {
    CHECK(std::get<std::string>(expr::evaluate(R"(format(3.14159, "{:.2f}"))", ctx)) == "3.14");
    CHECK(std::get<std::string>(expr::evaluate(R"(format(42))", ctx)) == "42");
    CHECK(std::get<std::string>(expr::evaluate(R"(42 | format("0x{:x}"))", ctx)) == "0x2a");

    CHECK_THROWS_AS(expr::evaluate("format()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("format(1, 2)", ctx), std::runtime_error);
  }

  SUBCASE("render end to end")
  {
    CHECK(jinja::render(jinja::parse("{% for i in range(3) %}{{ i }}{% endfor %}"), ctx) == "012");
  }
}
