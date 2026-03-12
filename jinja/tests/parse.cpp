#include <doctest/doctest.h>

import reflex.jinja;
import std;

using namespace reflex;
using namespace reflex::jinja;
using namespace reflex::serde;

using namespace std::string_literals;

#define JINJA(...) #__VA_ARGS__

TEST_CASE("reflex::jinja: parse")
{
  SUBCASE("basic expression")
  {
    auto tmpl = jinja::parse("hello {{world}}");

    jinja::basic_context ctx;
    ctx["world"] = "reflex"s;
    auto result  = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "hello reflex");
  }
  SUBCASE("for loop")
  {
    auto tmpl = jinja::parse(JINJA({% for item in items %}- {{item}}\n{% endfor %}));

    jinja::json_context ctx;
    ctx["items"] = serde::json::array{"banana", "apple", "cherry"};

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == R"(- banana
- apple
- cherry
)");
  }
  SUBCASE("if/else")
  {
    jinja::json_context ctx;
    ctx["condition"] = true;
    auto tmpl        = jinja::parse(
        R"({% if condition %}Condition is true{% else %}Condition is false{% endif %})");
    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "Condition is true");
    ctx["condition"] = false;
    result           = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "Condition is false");
  }

  SUBCASE("if/elseif")
  {
    auto tmpl = jinja::parse(
        R"({% if condition == "a" %}A{% elif condition == "b" %}B{% else %}Unknown{% endif %})");

    jinja::basic_context ctx;
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
  SUBCASE("single var over array (existing behaviour)")
  {
    auto         tmpl = parse("{% for item in items %}{{ item }} {% endfor %}");
    json_context ctx;
    ctx.set(
        "items", json::value{
                     json::array{std::string{"a"}, std::string{"b"}, std::string{"c"}}
    });

    CHECK(render(tmpl, ctx) == "a b c ");
  }

  SUBCASE("k, v decomposition over object")
  {
    auto         tmpl = parse("{% for k, v in obj %}{{ k }}={{ v }}\n{% endfor %}");
    json_context ctx;
    ctx.set(
        "obj", json::value{
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

    json_context ctx;
    ctx.set(
        "data", json::value{
                    {"enabled",  true },
                    {"disabled", false}
    });

    auto result = render(tmpl, ctx);
    CHECK(result == "enabled: true\n");
  }

  SUBCASE("single-var over object gives keys")
  {
    auto         tmpl = parse("{% for v in obj %}{{ v }} {% endfor %}");
    json_context ctx;
    ctx.set(
        "obj", json::value{
                   {"k", std::string{"val"}}
    });
    CHECK(render(tmpl, ctx) == "k ");
  }
}