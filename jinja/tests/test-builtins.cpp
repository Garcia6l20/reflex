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

  SUBCASE("tojson")
  {
    // The exact bytes are a format contract, so a serde change is caught here.
    ctx.set(
        "o", {
                 {"a", 1}
    });
    CHECK(std::get<std::string>(expr::evaluate("tojson(o)", ctx)) == R"({"a":1})");

    ctx.set("xs", array{1, 2});
    CHECK(std::get<std::string>(expr::evaluate("tojson(xs)", ctx)) == "[1,2]");

    CHECK(std::get<std::string>(expr::evaluate(R"(tojson("a\"b"))", ctx)) == R"("a\"b")");
    CHECK(std::get<std::string>(expr::evaluate("tojson(42)", ctx)) == "42");
    CHECK(std::get<std::string>(expr::evaluate("tojson(true)", ctx)) == "true");
    CHECK(std::get<std::string>(expr::evaluate("tojson(null)", ctx)) == "null");

    CHECK_THROWS_AS(expr::evaluate("tojson()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("tojson(1, 2)", ctx), std::runtime_error);
  }

  SUBCASE("render end to end")
  {
    CHECK(jinja::render(jinja::parse("{% for i in range(3) %}{{ i }}{% endfor %}"), ctx) == "012");
  }
}

TEST_CASE("reflex::jinja: builtins tier 2, strings")
{
  basic_context ctx;
  const auto    str = [&](std::string_view src) {
    return std::get<std::string>(expr::evaluate(src, ctx));
  };

  SUBCASE("upper and lower")
  {
    CHECK(str(R"(upper("aB1"))") == "AB1");
    CHECK(str(R"(upper(""))") == "");
    CHECK(str(R"(lower("Ab1"))") == "ab1");
    // non-ASCII passes through unchanged
    CHECK(str(R"(upper("é"))") == "é");

    CHECK_THROWS_AS(expr::evaluate("upper()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("upper(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("lower()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("lower(1)", ctx), std::runtime_error);
  }

  SUBCASE("capitalize")
  {
    CHECK(str(R"(capitalize("hELLO"))") == "Hello");
    CHECK(str(R"(capitalize(""))") == "");
    CHECK_THROWS_AS(expr::evaluate("capitalize()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("capitalize(1)", ctx), std::runtime_error);
  }

  SUBCASE("trim")
  {
    CHECK(str(R"(trim("  x  "))") == "x");
    CHECK(str(R"(trim("   "))") == "");
    CHECK(str(R"(trim("xxaxx", "x"))") == "a");
    CHECK(str(R"(trim("abc", ""))") == "abc");

    CHECK_THROWS_AS(expr::evaluate("trim()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(trim("a", "b", "c"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("trim(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(trim("a", 1))", ctx), std::runtime_error);
  }

  SUBCASE("replace")
  {
    CHECK(str(R"(replace("aaa", "a", "b"))") == "bbb");
    CHECK(str(R"(replace("aa", "aa", ""))") == "");
    CHECK(str(R"(replace("abc", "z", "y"))") == "abc");

    CHECK_THROWS_AS(expr::evaluate(R"(replace("a", "", "b"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(replace("a", "b"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(replace(1, "a", "b"))", ctx), std::runtime_error);
  }

  SUBCASE("split")
  {
    // no separator: runs of whitespace, empty fields dropped
    CHECK(str(R"(join(split("a b  c"), "|"))") == "a|b|c");
    CHECK(str(R"(join(split("  a  "), "|"))") == "a");
    CHECK(std::get<int>(expr::evaluate(R"(length(split("   ")))", ctx)) == 0);

    // explicit separator: exact, empty fields kept
    CHECK(str(R"(join(split("a,,b", ","), "|"))") == "a||b");
    CHECK(str(R"(join(split("a b  c", " "), "|"))") == "a|b||c");
    CHECK(str(R"(join(split("abc", "-"), "|"))") == "abc");

    CHECK_THROWS_AS(expr::evaluate(R"(split("a", ""))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("split()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("split(1)", ctx), std::runtime_error);
  }

  SUBCASE("startswith and endswith")
  {
    CHECK(std::get<bool>(expr::evaluate(R"(startswith("abc", "ab"))", ctx)) == true);
    CHECK(std::get<bool>(expr::evaluate(R"(startswith("abc", "bc"))", ctx)) == false);
    CHECK(std::get<bool>(expr::evaluate(R"(endswith("abc", "bc"))", ctx)) == true);
    CHECK(std::get<bool>(expr::evaluate(R"(endswith("abc", "ab"))", ctx)) == false);

    CHECK_THROWS_AS(expr::evaluate(R"(startswith("abc"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(startswith("abc", 1))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(endswith("abc"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(endswith(1, "a"))", ctx), std::runtime_error);
  }

  SUBCASE("indent")
  {
    ctx.set("body", "a\nb"s);
    CHECK(str("indent(body, 2)") == "a\n  b");
    CHECK(str("indent(body, 2, true)") == "  a\n  b");

    // a blank line is not indented, so no line gains trailing whitespace
    ctx.set("gapped", "a\n\nb"s);
    CHECK(str("indent(gapped, 2)") == "a\n\n  b");

    ctx.set("trailing", "a\nb\n"s);
    CHECK(str("indent(trailing, 2)") == "a\n  b\n");

    CHECK_THROWS_AS(expr::evaluate("indent(body)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(indent(body, "2"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("indent(body, -1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("indent(1, 2)", ctx), std::runtime_error);
  }

  SUBCASE("truncate")
  {
    CHECK(str(R"(truncate("abcdef", 5))") == "ab...");
    CHECK(str(R"(truncate("ab", 5))") == "ab");
    CHECK(str(R"(truncate("abcdef", 4, "!"))") == "abc!");

    CHECK_THROWS_AS(expr::evaluate(R"(truncate("abcdef", 2))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(truncate("abcdef"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(truncate("abcdef", "2"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("truncate(1, 2)", ctx), std::runtime_error);
  }

  SUBCASE("render end to end strings")
  {
    ctx.set("names", array{"alpha"s, "beta"s});
    const auto tmpl = jinja::parse(
        "struct s {\n"
        "{% for n in names %}{{ format(upper(n), \"int {};\") | indent(2, true) }}\n"
        "{% endfor %}"
        "};");
    CHECK(jinja::render(tmpl, ctx) == "struct s {\n  int ALPHA;\n  int BETA;\n};");
  }
}

TEST_CASE("reflex::jinja: builtins tier 3, case conversion")
{
  basic_context ctx;
  const auto    str = [&](std::string_view src) {
    return std::get<std::string>(expr::evaluate(src, ctx));
  };

  SUBCASE("the five converters")
  {
    CHECK(str(R"(snake("myField"))") == "my_field");
    CHECK(str(R"(camel("my_field"))") == "myField");
    CHECK(str(R"(pascal("my_field"))") == "MyField");
    CHECK(str(R"(kebab("myField"))") == "my-field");
    CHECK(str(R"(upper_snake("myField"))") == "MY_FIELD");
  }

  SUBCASE("idempotent on already-converted input, as core asserts")
  {
    CHECK(str(R"(snake("hello_world"))") == "hello_world");
    CHECK(str(R"(camel("helloWorld"))") == "helloWorld");
    CHECK(str(R"(pascal("HelloWorld"))") == "HelloWorld");
    CHECK(str(R"(kebab("hello-world"))") == "hello-world");
    CHECK(str(R"(upper_snake("HELLO_WORLD"))") == "HELLO_WORLD");
  }

  SUBCASE("empty input returns empty")
  {
    CHECK(str(R"(snake(""))") == "");
    CHECK(str(R"(camel(""))") == "");
    CHECK(str(R"(pascal(""))") == "");
    CHECK(str(R"(kebab(""))") == "");
    CHECK(str(R"(upper_snake(""))") == "");
  }

  SUBCASE("the to_*_case aliases are the same function")
  {
    CHECK(str(R"(to_snake_case("myField"))") == str(R"(snake("myField"))"));
    CHECK(str(R"(to_camel_case("my_field"))") == str(R"(camel("my_field"))"));
    CHECK(str(R"(to_pascal_case("my_field"))") == str(R"(pascal("my_field"))"));
    CHECK(str(R"(to_kebab_case("myField"))") == str(R"(kebab("myField"))"));
    CHECK(str(R"(to_upper_snake_case("myField"))") == str(R"(upper_snake("myField"))"));
  }

  SUBCASE("as pipes")
  {
    CHECK(str(R"("myField" | snake)") == "my_field");
    CHECK(str(R"("myField" | to_upper_snake_case)") == "MY_FIELD");
  }

  SUBCASE("wrong arity and wrong type")
  {
    CHECK_THROWS_AS(expr::evaluate("snake()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(snake("a", "b"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("camel(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("pascal(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("kebab(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("upper_snake(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("to_snake_case()", ctx), std::runtime_error);

    // an alias reports the short name, since both spellings share one callable
    CHECK_THROWS_WITH(expr::evaluate("to_snake_case()", ctx), doctest::Contains("snake"));
  }

  SUBCASE("a user def still wins")
  {
    ctx.def("snake", [](std::span<const value>) -> value { return "user"s; });
    CHECK(str(R"(snake("myField"))") == "user");
    // the alias is a separate key, so it keeps the builtin
    CHECK(str(R"(to_snake_case("myField"))") == "my_field");
  }
}

TEST_CASE("reflex::jinja: builtins tier 4, numeric")
{
  basic_context ctx;

  SUBCASE("abs preserves the alternative")
  {
    CHECK(std::get<int>(expr::evaluate("abs(-7)", ctx)) == 7);
    CHECK(std::get<int>(expr::evaluate("abs(7)", ctx)) == 7);
    CHECK(std::get<double>(expr::evaluate("abs(-1.5)", ctx)) == doctest::Approx(1.5));

    CHECK_THROWS_AS(expr::evaluate("abs()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("abs(1, 2)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(abs("x"))", ctx), std::runtime_error);
  }

  SUBCASE("round always returns a double")
  {
    CHECK(std::get<double>(expr::evaluate("round(3.14159, 2)", ctx)) == doctest::Approx(3.14));
    // ties round away from zero, which is what std::round does
    CHECK(std::get<double>(expr::evaluate("round(2.5)", ctx)) == doctest::Approx(3.0));
    CHECK(std::get<double>(expr::evaluate("round(-2.5)", ctx)) == doctest::Approx(-3.0));
    CHECK(std::get<double>(expr::evaluate("round(7)", ctx)) == doctest::Approx(7.0));

    CHECK_THROWS_AS(expr::evaluate("round()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("round(1, 2, 3)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(round("x"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(round(1.5, "2"))", ctx), std::runtime_error);
  }

  SUBCASE("int")
  {
    CHECK(std::get<int>(expr::evaluate(R"(int("42"))", ctx)) == 42);
    CHECK(std::get<int>(expr::evaluate("int(3.9)", ctx)) == 3);
    CHECK(std::get<int>(expr::evaluate("int(-3.9)", ctx)) == -3);
    CHECK(std::get<int>(expr::evaluate("int(true)", ctx)) == 1);
    CHECK(std::get<int>(expr::evaluate("int(false)", ctx)) == 0);
    CHECK(std::get<int>(expr::evaluate("int(7)", ctx)) == 7);

    // a strict parse: trailing junk is an error, not a prefix
    CHECK_THROWS_AS(expr::evaluate(R"(int("x"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(int("12abc"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("int(null)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("int()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("int(1, 2)", ctx), std::runtime_error);
  }

  SUBCASE("float")
  {
    CHECK(std::get<double>(expr::evaluate(R"(float("1.5"))", ctx)) == doctest::Approx(1.5));
    CHECK(std::get<double>(expr::evaluate("float(2)", ctx)) == doctest::Approx(2.0));
    CHECK(std::get<double>(expr::evaluate("float(1.5)", ctx)) == doctest::Approx(1.5));
    CHECK(std::get<double>(expr::evaluate("float(true)", ctx)) == doctest::Approx(1.0));

    CHECK_THROWS_AS(expr::evaluate(R"(float("x"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(float("1.5x"))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("float(null)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("float()", ctx), std::runtime_error);
  }

  SUBCASE("string")
  {
    CHECK(std::get<std::string>(expr::evaluate("string(42)", ctx)) == "42");
    CHECK(std::get<std::string>(expr::evaluate("string(true)", ctx)) == "true");
    CHECK(std::get<std::string>(expr::evaluate(R"(string("a"))", ctx)) == "a");
    CHECK(std::get<std::string>(expr::evaluate("string(null)", ctx)) == "null");

    CHECK_THROWS_AS(expr::evaluate("string()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("string(1, 2)", ctx), std::runtime_error);
  }

  SUBCASE("a variable may be called string")
  {
    // identifiers and calls are distinct tokens, so a builtin name is not reserved
    ctx.set("string", 7);
    CHECK(std::get<int>(expr::evaluate("string", ctx)) == 7);
    CHECK(std::get<std::string>(expr::evaluate("string(1)", ctx)) == "1");
  }

  SUBCASE("sum")
  {
    ctx.set("empty", array{});
    CHECK(std::get<int>(expr::evaluate("sum(empty)", ctx)) == 0);

    ctx.set("ints", array{1, 2});
    CHECK(std::get<int>(expr::evaluate("sum(ints)", ctx)) == 3);

    ctx.set("mixed", array{1, 2.5});
    CHECK(std::get<double>(expr::evaluate("sum(mixed)", ctx)) == doctest::Approx(3.5));

    ctx.set("strs", array{"a"s});
    CHECK_THROWS_AS(expr::evaluate("sum(strs)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("sum(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("sum()", ctx), std::runtime_error);
  }

  SUBCASE("min and max take a sequence")
  {
    ctx.set("xs", array{3, 1, 2});
    CHECK(std::get<int>(expr::evaluate("min(xs)", ctx)) == 1);
    CHECK(std::get<int>(expr::evaluate("max(xs)", ctx)) == 3);
    CHECK(std::get<int>(expr::evaluate("xs | max", ctx)) == 3);

    ctx.set("empty", array{});
    CHECK_THROWS_AS(expr::evaluate("min(empty)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("max(empty)", ctx), std::runtime_error);

    // numeric only, including the single-element case
    ctx.set("one_str", array{"a"s});
    CHECK_THROWS_AS(expr::evaluate("min(one_str)", ctx), std::runtime_error);
    ctx.set("strs", array{"a"s, "b"s});
    CHECK_THROWS_AS(expr::evaluate("max(strs)", ctx), std::runtime_error);

    CHECK_THROWS_AS(expr::evaluate("min(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("min(xs, 1)", ctx), std::runtime_error);
  }

  SUBCASE("render end to end numeric")
  {
    ctx.set("xs", array{1, 2, 3, 4});
    // `/` on two ints is integer division, so the average needs the float() cast
    CHECK(jinja::render(jinja::parse("{{ sum(xs) / length(xs) }}"), ctx) == "2");
    CHECK(
        jinja::render(jinja::parse("{{ round(float(sum(xs)) / length(xs), 2) }}"), ctx) == "2.5");
  }
}

TEST_CASE("reflex::jinja: builtins tier 4, sequences and objects")
{
  basic_context ctx;
  const auto    str = [&](std::string_view src) {
    return std::get<std::string>(expr::evaluate(src, ctx));
  };

  SUBCASE("first and last")
  {
    ctx.set("xs", array{1, 2});
    CHECK(std::get<int>(expr::evaluate("first(xs)", ctx)) == 1);
    CHECK(std::get<int>(expr::evaluate("last(xs)", ctx)) == 2);
    CHECK(str(R"(first("ab"))") == "a");
    CHECK(str(R"(last("ab"))") == "b");

    ctx.set("empty", array{});
    CHECK_THROWS_AS(expr::evaluate("first(empty)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("last(empty)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate(R"(first(""))", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("first(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("first()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("first(xs, 1)", ctx), std::runtime_error);

    // first() throws before default() ever runs, so the two do not compose
    CHECK_THROWS_AS(expr::evaluate(R"(empty | first | default("none"))", ctx), std::runtime_error);
  }

  SUBCASE("keys and values are key-sorted and aligned")
  {
    ctx.set(
        "o", {
                 {"b", 1},
                 {"a", 2}
    });
    CHECK(str(R"(join(keys(o), ","))") == "a,b");
    CHECK(str(R"(join(values(o), ","))") == "2,1");
    CHECK(std::get<int>(expr::evaluate("length(keys(o))", ctx)) == 2);

    CHECK_THROWS_AS(expr::evaluate("keys(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("values(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("keys()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("values(o, 1)", ctx), std::runtime_error);
  }

  SUBCASE("items")
  {
    ctx.set(
        "o", {
                 {"b", 1},
                 {"a", 2}
    });
    CHECK(std::get<int>(expr::evaluate("length(items(o))", ctx)) == 2);
    CHECK(str(R"(join(first(items(o)), "="))") == "a=2");
    CHECK(str(R"(join(last(items(o)), "="))") == "b=1");

    CHECK_THROWS_AS(expr::evaluate("items(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("items()", ctx), std::runtime_error);

    CHECK(
        jinja::render(
            jinja::parse(R"({% for p in items(o) %}{{ p[0] }}={{ p[1] }};{% endfor %})"), ctx)
        == "a=2;b=1;");
    CHECK(
        jinja::render(
            jinja::parse(R"({% for p in items(o) %}{{ first(p) }}={{ last(p) }};{% endfor %})"),
            ctx)
        == "a=2;b=1;");
  }

  SUBCASE("unique preserves order, first occurrence wins")
  {
    ctx.set("xs", array{1, 1, 2, 1});
    CHECK(str(R"(join(unique(xs), ","))") == "1,2");

    ctx.set("strs", array{"a"s, "a"s});
    CHECK(str(R"(join(unique(strs), ","))") == "a");

    ctx.set("empty", array{});
    CHECK(std::get<int>(expr::evaluate("length(unique(empty))", ctx)) == 0);

    CHECK_THROWS_AS(expr::evaluate("unique(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("unique()", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("unique(xs, 1)", ctx), std::runtime_error);
  }

  SUBCASE("sort orders numbers and strings, never a mix")
  {
    ctx.set("nums", array{3, 1, 2});
    CHECK(str(R"(join(sort(nums), ","))") == "1,2,3");

    ctx.set("mixed_nums", array{3, 1.5});
    CHECK(str(R"(join(sort(mixed_nums), ","))") == "1.5,3");

    ctx.set("strs", array{"b"s, "a"s});
    CHECK(str(R"(join(sort(strs), ","))") == "a,b");

    ctx.set("empty", array{});
    CHECK(std::get<int>(expr::evaluate("length(sort(empty))", ctx)) == 0);

    ctx.set("mix", array{1, "a"s});
    CHECK_THROWS_AS(expr::evaluate("sort(mix)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("sort(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("sort()", ctx), std::runtime_error);
  }

  SUBCASE("natsort orders digit runs numerically")
  {
    // the case the whole filter exists for: lexicographic order puts PA10 first
    ctx.set("pins", array{"PA10"s, "PA2"s});
    CHECK(str(R"(join(natsort(pins), ","))") == "PA2,PA10");
    CHECK(str(R"(join(sort(pins), ","))") == "PA10,PA2");

    // case-insensitive
    ctx.set("cased", array{"X10"s, "x2"s});
    CHECK(str(R"(join(natsort(cased), ","))") == "x2,X10");

    // leading zeros: the runs compare equal after stripping, so the order is left as found
    ctx.set("zeros", array{"a01"s, "a1"s});
    CHECK(str(R"(join(natsort(zeros), ","))") == "a01,a1");

    ctx.set("empty", array{});
    CHECK(std::get<int>(expr::evaluate("length(natsort(empty))", ctx)) == 0);

    CHECK_THROWS_AS(expr::evaluate("natsort(1)", ctx), std::runtime_error);
    CHECK_THROWS_AS(expr::evaluate("natsort()", ctx), std::runtime_error);
  }

  SUBCASE("render end to end")
  {
    ctx.set("pins", array{"PA10"s, "PA2"s, "PA2"s});
    CHECK(
        jinja::render(jinja::parse("{% for p in natsort(unique(pins)) %}{{ p }};{% endfor %}"), ctx)
        == "PA2;PA10;");
  }
}
