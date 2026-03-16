#include <doctest/doctest.h>

import reflex.jinja;
import std;

using namespace reflex;
using namespace reflex::jinja;
using namespace reflex::serde;

using namespace std::string_literals;

#define JINJA(...) #__VA_ARGS__

using value  = jinja::basic_context::value_type;
using object = jinja::basic_context::object_type;
using array  = jinja::basic_context::array_type;

TEST_CASE("reflex::jinja: parse")
{
  jinja::basic_context ctx;

  SUBCASE("basic expression")
  {
    auto tmpl = jinja::parse("hello {{world}}");

    ctx.set("world", "reflex"s);
    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "hello reflex");
  }
  SUBCASE("for loop")
  {
    auto tmpl = jinja::parse(
        "{% for item in items %}"
        "{% if not loop.first %}, {% endif %}{{ loop.index }}: {{item}}"
        "{% endfor %}");

    ctx.set("items", array{"banana"s, "apple"s, "cherry"s});

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == R"(1: banana, 2: apple, 3: cherry)");
  }
  SUBCASE("nested for loop")
  {
    auto tmpl = jinja::parse(
        "{% for row in table %}{% for cell in row %}"
        "[{{loop.parent.index}},{{loop.index}}] = {{ cell }}\n"
        "{% endfor %}{% endfor %}");

    ctx.set(
        "table", array{
                     array{1, 2, 3},
                     array{4, 5, 6},
                     array{7, 8, 9},
    });

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == R"([1,1] = 1
[1,2] = 2
[1,3] = 3
[2,1] = 4
[2,2] = 5
[2,3] = 6
[3,1] = 7
[3,2] = 8
[3,3] = 9
)");
  }
  SUBCASE("if/else")
  {
    ctx.set("condition", true);
    auto tmpl = jinja::parse(
        R"({% if condition %}Condition is true{% else %}Condition is false{% endif %})");
    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "Condition is true");
    ctx.set("condition", false);
    result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "Condition is false");
  }

  SUBCASE("if/elseif")
  {
    auto tmpl = jinja::parse(
        R"({% if condition == "a" %}A{% elif condition == "b" %}B{% else %}Unknown{% endif %})");

    ctx.set("condition", "a"s);

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "A");
    ctx.set("condition", "b"s);
    result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "B");
    ctx.set("condition", "c"s);
    result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "Unknown");
  }
}

TEST_CASE("reflex::jinja: for decomposition")
{
  basic_context ctx;

  SUBCASE("single var over array (existing behaviour)")
  {
    auto tmpl = parse("{% for item in items %}{{ item }} {% endfor %}");
    ctx.set(
        "items", value{
                     array{"a"s, "b"s, "c"s}
    });

    CHECK(render(tmpl, ctx) == "a b c ");
  }

  SUBCASE("k, v decomposition over object")
  {
    auto tmpl = parse("{% for k, v in obj %}{{ k }}={{ v }}\n{% endfor %}");
    ctx.set(
        "obj", value{
                   {"a", 1},
                   {"b", 2},
                   {"c", 3}
    });

    auto result = render(tmpl, ctx);
    CHECK(result == "a=1\nb=2\nc=3\n");
  }

  SUBCASE("decomposition inside nested template")
  {
    auto tmpl = parse(
        "{% for k, v in data %}"
        "{% if v %}"
        "{{ k }}: {{ v }}\n"
        "{% endif %}"
        "{% endfor %}");

    basic_context ctx;
    ctx.set(
        "data", value{
                    {"enabled",  true },
                    {"disabled", false}
    });

    auto result = render(tmpl, ctx);
    CHECK(result == "enabled: true\n");
  }

  SUBCASE("single-var over object gives keys")
  {
    auto          tmpl = parse("{% for v in obj %}{{ v }} {% endfor %}");
    basic_context ctx;
    ctx.set(
        "obj", value{
                   {"k", "val"s}
    });
    CHECK(render(tmpl, ctx) == "k ");
  }
}

using namespace reflex::literals;

TEST_CASE("reflex::jinja: aggregate support")
{
  struct aggregate
  {
    int         a;
    std::string b;
  };
  aggregate agg{42, "hello"s};
  auto      ctx = expr::context{"agg"_na = agg};

  auto tmpl   = parse("a={{ agg.a }}, b={{ agg.b }}");
  auto result = render(tmpl, ctx);
  CHECK(result == "a=42, b=hello");
}
