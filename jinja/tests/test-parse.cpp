#include <doctest/doctest.h>

import jinja.tests.types;
import reflex.jinja;
import std;

using namespace reflex;
using namespace reflex::jinja;
using namespace reflex::serde;

using namespace reflex::literals;
using namespace std::string_literals;

using namespace testing;

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
  SUBCASE("for loop - trimming whitespace")
  {
    auto tmpl = jinja::parse(
        "\n"
        "{%- for item in items -%}\n"
        "\t{{item}}\n"
        "{%- endfor -%}\n");

    ctx.set("items", array{"banana"s, "apple"s, "cherry"s});

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == R"(bananaapplecherry)");
  }
  SUBCASE("for loop - trimming whitespace 2")
  {
    auto tmpl = jinja::parse(
        "\n"
        "{%- for item in items -%}\n"
        "{{item.a}}\n"
        "{{item.b}}\n"
        "{%- endfor -%}\n");

    ctx.set(
        "items", array{
                     object{
                            {"a", "A"s},
                            {"b", "B"s},
                            },
                     object{
                            {"a", "C"s},
                            {"b", "D"s},
                            }
    });

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "A\nBC\nD");
  }
  SUBCASE("if block - trimming whitespace")
  {
    ctx.set("enabled", true);

    auto tmpl = jinja::parse(
        "  \n"
        "{%- if enabled -%}\n"
        "  ok\n"
        "{%- endif -%}\n"
        "done");

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "okdone");
  }
  SUBCASE("if block - trimming whitespace keeps before and after")
  {
    ctx.set("enabled", true);

    auto tmpl = jinja::parse(
        "\n"
        "{% if enabled -%}\n"
        "  ok\n"
        "{%- endif %}\n"
        "done");

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "\nok\ndone");
  }
  SUBCASE("if block - trimming whitespace keeps inner")
  {
    ctx.set("enabled", true);

    auto tmpl = jinja::parse(
        "\n"
        "{%- if enabled %}\n"
        "  ok\n"
        "{% endif -%}\n"
        "done");

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "\n  ok\ndone");
  }
  SUBCASE("if block - trimming whitespace keeps inner render blocks")
  {
    ctx.set("a", "A"s).set("b", "B"s).set("enabled", true);

    auto tmpl = jinja::parse(
        "\n"
        "{%- if enabled -%}\n"
        "  {{ a }}\n"
        "  {{ b }}\n"
        "{%- endif -%}\n"
        "done");

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "A\n  Bdone");
  }
  SUBCASE("if block - trimming whitespace with else branch")
  {
    ctx.set("enabled", false);

    auto tmpl = jinja::parse(
        "A"
        "{%- if enabled -%}\n"
        "  yes\n"
        "{%- else -%}\n"
        "  no\n"
        "{%- endif -%}"
        "B");

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "AnoB");
  }
  SUBCASE("if block - trimming whitespace with elif branch")
  {
    ctx.set("condition", "b"s);

    auto tmpl = jinja::parse(
        "{%- if condition == \"a\" -%}\n"
        "  A\n"
        "{%- elif condition == \"b\" -%}\n"
        "  B\n"
        "{%- else -%}\n"
        "  C\n"
        "{%- endif -%}");

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "B");
  }
  SUBCASE("if block - trimming whitespace with empty body")
  {
    ctx.set("enabled", true);

    auto tmpl = jinja::parse("before{%- if enabled -%}{%- endif -%}after");

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "beforeafter");
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
    CHECK(
        result
        == "[1,1] = 1\n"
           "[1,2] = 2\n"
           "[1,3] = 3\n"
           "[2,1] = 4\n"
           "[2,2] = 5\n"
           "[2,3] = 6\n"
           "[3,1] = 7\n"
           "[3,2] = 8\n"
           "[3,3] = 9\n");
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
        R"({% if condition == "a" %}A{% elif condition == "b" %}B{% else %}Unknown{% endif
          %})");

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

TEST_CASE("reflex::jinja: set block")
{
  basic_context ctx;

  SUBCASE("basic literal")
  {
    auto tmpl   = jinja::parse(R"({% set greeting = "hello" %}{{ greeting }})");
    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "hello");
  }
  SUBCASE("no spaces around =")
  {
    auto tmpl   = jinja::parse(R"({% set x=5 %}{{ x }})");
    auto result = jinja::render(tmpl, ctx);
    CHECK(result == "5");
  }
  SUBCASE("expression with arithmetic")
  {
    ctx.set("a", 3);
    ctx.set("b", 4);
    auto tmpl   = jinja::parse(R"({% set n = a + b %}{{ n }})");
    auto result = jinja::render(tmpl, ctx);
    CHECK(result == "7");
  }
  SUBCASE("rhs contains == (split on assignment, not equality)")
  {
    ctx.set("a", "x"s);
    ctx.set("b", "x"s);
    auto tmpl   = jinja::parse(R"({% set same = a == b %}{{ same }})");
    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "true");
  }
  SUBCASE("value is reusable across the template")
  {
    ctx.set("base", 10);
    auto tmpl = jinja::parse(
        R"({% set doubled = base * 2 %}{{ doubled }} {% if doubled > base %}bigger{% endif %} {{ doubled }})");
    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "20 bigger 20");
  }
  SUBCASE("whitespace trimming")
  {
    auto tmpl   = jinja::parse("{%- set x = 1 -%}  {{ x }}");
    auto result = jinja::render(tmpl, ctx);
    CHECK(result == "1");
  }
  SUBCASE("a set inside a loop does not outlive it")
  {
    ctx.set("items", array{1, 2});
    auto tmpl = jinja::parse(
        R"({% for i in items %}{% set doubled = i * 2 %}{{ doubled }} {% endfor %}[{{ doubled }}])");
    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "2 4 [null]");
  }
  SUBCASE("a set inside a loop shadows the outer binding only within it")
  {
    ctx.set("items", array{1, 2});
    auto tmpl = jinja::parse(
        R"({% set x = "outer" %}{% for i in items %}{% set x = i %}{{ x }} {% endfor %}{{ x }})");
    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "1 2 outer");
  }
  SUBCASE("a top-level set lands in the context globals")
  {
    auto tmpl = jinja::parse(R"({% set top = 1 %}{{ top }})");
    CHECK(jinja::render(tmpl, ctx) == "1");
    CHECK(std::get<int>(ctx["top"]) == 1);
  }
}

TEST_CASE("reflex::jinja: for decomposition")
{
  basic_context ctx;

  SUBCASE("single var over array (existing behaviour)")
  {
    auto tmpl = jinja::parse("{% for item in items %}{{ item }} {% endfor %}");
    ctx.set(
        "items", value{
                     array{"a"s, "b"s, "c"s}
    });

    CHECK(render(tmpl, ctx) == "a b c ");
  }

  SUBCASE("k, v decomposition over object")
  {
    auto tmpl = jinja::parse("{% for k, v in obj %}{{ k }}={{ v }}\n{% endfor %}");
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
    auto tmpl = jinja::parse(
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
    auto          tmpl = jinja::parse("{% for v in obj %}{{ v }} {% endfor %}");
    basic_context ctx;
    ctx.set(
        "obj", value{
                   {"k", "val"s}
    });
    CHECK(render(tmpl, ctx) == "k ");
  }
}

// {{ a.b }} resolves its leading dotted chain with serde::object_visit, and the
// object it walks is the whole variable scope. A segment landing on a value with
// no members must not hand that scope back, or a template asking for a member of
// a number renders every variable in scope.
//
// It reads as an undefined name instead, which is what context::visit's scope
// search is written around: it is noexcept, so it cannot be told about a bad
// path any other way.
TEST_CASE("reflex::jinja: a dotted name reaching through a scalar is undefined")
{
  jinja::basic_context ctx;
  ctx.set("n", 42);
  ctx.set("s", "text"s);

  const auto undefined = render(jinja::parse("{{ nosuchvar }}"), ctx);

  SUBCASE("a member of a number")
  {
    CHECK(render(jinja::parse("{{ n.b }}"), ctx) == undefined);
    // The scope had two variables in it and neither reached the output.
    CHECK(render(jinja::parse("{{ n.b }}"), ctx).find("text") == std::string::npos);
  }
  SUBCASE("a member of a string")
  {
    CHECK(render(jinja::parse("{{ s.length }}"), ctx) == undefined);
  }
  SUBCASE("several segments in")
  {
    CHECK(render(jinja::parse("{{ n.a.b.c }}"), ctx) == undefined);
  }
  SUBCASE("the names that do resolve are unaffected")
  {
    CHECK(render(jinja::parse("{{ n }}/{{ s }}"), ctx) == "42/text");
  }
  SUBCASE("a member of a bound aggregate still resolves")
  {
    aggregate2 agg{
        3.14, {42, "world"s}
    };
    auto agg_ctx = expr::context{"agg"_na = agg};
    CHECK(render(jinja::parse("{{ agg.nested.a }}"), agg_ctx) == "42");
    // The member is there, the path through it is not.
    CHECK(render(jinja::parse("{{ agg.nested.a.deeper }}"), agg_ctx)
          == render(jinja::parse("{{ nosuchvar }}"), agg_ctx));
  }
  SUBCASE("an optional member resolves to its payload")
  {
    aggregate4 engaged{
        true, aggregate2{3.14, {42, "world"s}}
    };
    auto ctx = expr::context{"agg"_na = engaged};

    CHECK(render(jinja::parse("{{ agg.optional_nested.nested.a }}"), ctx) == "42");

    bool unwrapped = false;
    ctx.visit("agg.optional_nested", [&unwrapped]<typename T>(T&&) {
      unwrapped =
          not meta::is_template_instance_of(dealias(^^std::remove_cvref_t<T>), ^^std::optional);
    });
    CHECK(unwrapped);
  }
  SUBCASE("a path reaching through a disengaged optional member is undefined")
  {
    aggregate4 empty{false, std::nullopt};
    auto       ctx = expr::context{"agg"_na = empty};

    const auto nothing = render(jinja::parse("{{ nosuchvar }}"), ctx);

    CHECK(render(jinja::parse("{{ agg.optional_nested.nested.a }}"), ctx) == nothing);
    CHECK(render(jinja::parse("{{ agg.optional_nested.nested.a.b.c }}"), ctx) == nothing);

    bool called = false;
    ctx.visit("agg.optional_nested.nested.a", [&called](auto&&) { called = true; });
    CHECK(not called);

    CHECK(render(jinja::parse("{{ agg.optional_nested }}"), ctx) == nothing);
    CHECK_THROWS_AS(render(jinja::parse("{{ agg.nosuchfield }}"), ctx), reflex::runtime_error);
  }
  SUBCASE("a path reaching through a null is undefined")
  {
    static_assert(not aggregate_c<poly::null_t>);
    static_assert(not serde::object_visitable_c<poly::null_t>);

    basic_context ctx;
    ctx.set("nul", poly::null);

    const auto nothing = render(jinja::parse("{{ nosuchvar }}"), ctx);

    CHECK(render(jinja::parse("{{ nul.b }}"), ctx) == nothing);
    CHECK(render(jinja::parse("{{ nul.b.c }}"), ctx) == nothing);
  }
}

TEST_CASE("reflex::jinja: aggregate support")
{
  SUBCASE("basic")
  {
    aggregate1 agg{42, "hello"s};
    auto       ctx = expr::context{"agg"_na = agg};

    auto tmpl   = jinja::parse("a={{ agg.a }}, b={{ agg.b }}");
    auto result = render(tmpl, ctx);
    CHECK(result == "a=42, b=hello");
  }

  SUBCASE("nested")
  {
    aggregate2 agg{
        3.14, {42, "world"s}
    };
    auto ctx = expr::context{"agg"_na = agg};

    auto tmpl   = jinja::parse("x={{ agg.x }}, a={{ agg.nested.a }}, b={{ agg.nested.b }}");
    auto result = render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "x=3.14, a=42, b=world");
  }
  SUBCASE("nested list")
  {
    aggregate3 agg{
        2.71, {{1, "one"s}, {2, "two"s}, {3, "three"s}}
    };
    auto ctx = expr::context{"agg"_na = agg};

    auto tmpl = jinja::parse(
        "x={{ agg.x }}\n"
        "{% for item in agg.nested_list %}a={{ item.a }}, b={{ item.b }}\n"
        "{% endfor %}");
    auto result = render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "x=2.71\na=1, b=one\na=2, b=two\na=3, b=three\n");
  }
  SUBCASE("list of aggregates")
  {
    aggregate5 agg{
        42,
        {{2.71, {{1, "one"s}, {2, "two"s}, {3, "three"s}}},
          {22.71, {{21, "twenty-one"s}, {22, "twenty-two"s}, {23, "twenty-three"s}}}}
    };
    auto ctx = expr::context{"agg"_na = agg};

    auto tmpl = jinja::parse(
        "{% for item in agg.nested_list[0].nested_list %}a={{ item.a }}, b={{ item.b }}\n"
        "{% endfor %}");
    auto result = render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "a=1, b=one\na=2, b=two\na=3, b=three\n");
  }
}

TEST_CASE("reflex::jinja: an unknown member of a bound aggregate throws")
{
  aggregate1 agg{42, "hello"s};
  auto       ctx  = expr::context{"agg"_na = agg};
  const auto tmpl = jinja::parse("{{ agg.nosuchfield }}");

  CHECK_THROWS_AS(render(tmpl, ctx), reflex::runtime_error);

  try
  {
    render(tmpl, ctx);
    FAIL("expected a throw");
  }
  catch(std::exception const& e)
  {
    CHECK(std::string_view{e.what()}.contains("agg.nosuchfield"));
  }

  SUBCASE("an unknown variable is still undefined rather than an error")
  {
    CHECK(render(jinja::parse("{{ nosuchvar }}"), ctx) == render(jinja::parse("{{ null }}"), ctx));
  }
}

TEST_CASE("reflex::jinja: an inner binding shadows an outer one for the whole dotted path")
{
  jinja::basic_context ctx;
  const auto           undefined = render(jinja::parse("{{ null }}"), ctx);

  SUBCASE("a local scalar hides an outer object of the same name")
  {
    ctx.set("a", value{
                     object{{"b", 7}}
    });
    ctx.set("items", array{5});
    auto tmpl = jinja::parse(R"({% for i in items %}{% set a = i %}{{ a.b }}|{{ a }}{% endfor %})");

    auto result = render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == undefined + "|5");
  }
  SUBCASE("with no outer binding the answer is the same")
  {
    ctx.set("items", array{5});
    auto tmpl = jinja::parse(R"({% for i in items %}{% set a = i %}{{ a.b }}{% endfor %})");

    CHECK(render(tmpl, ctx) == undefined);
  }
  SUBCASE("a failed lookup leaves every scope it searched alone")
  {
    ctx.set("a", value{
                     object{{"b", 7}}
    });
    auto guard = ctx.push_locals();
    guard.set("a", 5);

    CHECK(render(jinja::parse("{{ a.b }}"), ctx) == undefined);
    CHECK(render(jinja::parse("{{ nosuchvar.k }}"), ctx) == undefined);

    CHECK(ctx.local_vars.size() == 1);
    CHECK(ctx.local_vars.back().size() == 1);
    CHECK(not ctx.local_vars.back().contains("nosuchvar"));
    CHECK(ctx.global_vars.size() == 1);
    CHECK(not ctx.global_vars.contains("nosuchvar"));
    CHECK(std::get<int>(ctx.local_vars.back().at("a")) == 5);
    CHECK(std::get<object>(ctx.global_vars.at("a")).size() == 1);
  }
}

TEST_CASE("reflex::jinja: top-level bound containers")
{
  SUBCASE("vector")
  {
    std::vector<aggregate1> rows{
        {1, "one"s},
        {2, "two"s}
    };
    auto ctx = expr::context{"rows"_na = rows};

    auto tmpl   = jinja::parse("{% for r in rows %}{{ r.a }}:{{ r.b }} {% endfor %}");
    auto result = render(tmpl, ctx);
    CHECK(result == "1:one 2:two ");
  }

  SUBCASE("vector subscript")
  {
    std::vector<aggregate1> rows{
        {1, "one"s},
        {2, "two"s}
    };
    auto ctx = expr::context{"rows"_na = rows};

    auto result = render(jinja::parse("{{ rows[1].b }}"), ctx);
    CHECK(result == "two");
  }

  SUBCASE("map")
  {
    std::map<std::string, aggregate1> rows{
        {"first",  {1, "one"s}},
        {"second", {2, "two"s}}
    };
    auto ctx = expr::context{"rows"_na = rows};

    auto tmpl   = jinja::parse("{% for k, v in rows %}{{ k }}={{ v.a }} {% endfor %}");
    auto result = render(tmpl, ctx);
    CHECK(result == "first=1 second=2 ");
  }

  // A map answers to dotted access, the same as a poly::obj does. It did not
  // always: serde::object_visitable_c had no map specialization, so expr's
  // access_member fell through to "Cannot access key of non-object value".
  // Pinned because the behaviour is now deliberate rather than incidental.
  SUBCASE("map member access")
  {
    std::map<std::string, aggregate1> rows{
        {"first",  {1, "one"s}},
        {"second", {2, "two"s}}
    };
    auto ctx = expr::context{"rows"_na = rows};

    CHECK(render(jinja::parse("{{ rows.first.a }}"), ctx) == "1");
    CHECK(render(jinja::parse("{{ rows.second.b }}"), ctx) == "two");
    // A key the map does not hold is null, not an error - the same answer a
    // poly::obj gives.
    CHECK(render(jinja::parse("{% if rows.missing %}yes{% else %}no{% endif %}"), ctx) == "no");
  }

  SUBCASE("truthiness of a bound aggregate")
  {
    aggregate1 agg{0, ""s};
    auto       ctx = expr::context{"agg"_na = agg};

    CHECK(render(jinja::parse("{% if agg.a %}yes{% else %}no{% endif %}"), ctx) == "no");
  }
}

template <typename ValueT> ValueT jinja_format(std::span<ValueT const> args)
{
  if(args.size() > 2)
  {
    throw std::runtime_error("format(value, pattern) expects 2 arguments");
  }
  auto [fmt, arg] = [&] -> std::tuple<std::string_view, const ValueT*> {
    if(args.size() == 1)
    {
      return {"{}", &args[0]};
    }
    auto* fmt = std::get_if<std::string>(&args[1]);
    if(fmt == nullptr)
    {
      throw std::runtime_error("pattern must be a string");
    }
    return {*fmt, &args[0]};
  }();
  return reflex::visit(
      [fmt]<typename V>(V&& value) -> ValueT {
        using U = std::decay_t<V>;
        if constexpr(std::formattable<U, char>)
        {
          return std::vformat(fmt, std::make_format_args(value));
        }
        else
        {
          throw runtime_error(
              "Value of type {} is not formattable", display_string_of(dealias(^^U)));
        }
      },
      *arg);
}

TEST_CASE("reflex::jinja: pipe operator")
{
  jinja::basic_context ctx;
  using value_type = decltype(ctx)::value_type;
  ctx.set("value", 42).def("format", jinja_format<value_type>);

  SUBCASE("simple format")
  {
    auto tmpl   = jinja::parse(R"({{ value | format() }})");
    auto result = jinja::render(tmpl, ctx);
    CHECK(result == "42");
  }
  SUBCASE("format with spec")
  {
    auto tmpl   = jinja::parse(R"({{ value | format("0x{:x}") }})");
    auto result = jinja::render(tmpl, ctx);
    CHECK(result == "0x2a");
  }
}

TEST_CASE("reflex::jinja: pipe operator chaining")
{
  auto                 tmpl = jinja::parse(R"({{ value | add(2) | mul(3) | format("{}") }})");
  jinja::basic_context ctx;
  using value_type = decltype(ctx)::value_type;

  ctx.set("value", 4)
      .def(
          "add",
          [](std::span<const value_type> args) -> value_type {
            if(args.size() != 2)
            {
              throw std::runtime_error("add(value, rhs) expects 2 arguments");
            }

            auto* lhs = std::get_if<int>(&args[0]);
            auto* rhs = std::get_if<int>(&args[1]);
            if(lhs == nullptr or rhs == nullptr)
            {
              throw std::runtime_error("add(value, rhs) expects (int, int)");
            }
            return *lhs + *rhs;
          })
      .def(
          "mul",
          [](std::span<const value_type> args) -> value_type {
            if(args.size() != 2)
            {
              throw std::runtime_error("mul(value, rhs) expects 2 arguments");
            }

            auto* lhs = std::get_if<int>(&args[0]);
            auto* rhs = std::get_if<int>(&args[1]);
            if(lhs == nullptr or rhs == nullptr)
            {
              throw std::runtime_error("mul(value, rhs) expects (int, int)");
            }
            return *lhs * *rhs;
          })
      .def("format", jinja_format<value_type>);

  auto result = jinja::render(tmpl, ctx);
  CHECK(result == "18");
}

TEST_CASE("reflex::jinja: pipe operator edge cases")
{
  using context_type = jinja::basic_context;
  using value_type   = context_type::value_type;

  SUBCASE("unknown function")
  {
    auto         tmpl = jinja::parse(R"({{ value | missing_filter }})");
    context_type ctx;
    ctx.set("value", 42);
    CHECK_THROWS_AS(jinja::render(tmpl, ctx), std::runtime_error);
  }

  SUBCASE("invalid pipe target")
  {
    auto         tmpl = jinja::parse(R"({{ value | 123 }})");
    context_type ctx;
    ctx.set("value", 42);
    CHECK_THROWS_AS(jinja::render(tmpl, ctx), std::runtime_error);
  }

  SUBCASE("wrong arity")
  {
    auto         tmpl = jinja::parse(R"({{ value | format }})");
    context_type ctx;
    ctx.set("value", 42).def("format", [](std::span<const value_type> args) -> value_type {
      if(args.size() != 2)
      {
        throw std::runtime_error("format(value, pattern) expects 2 arguments");
      }
      return args[0];
    });

    CHECK_THROWS_AS(jinja::render(tmpl, ctx), std::runtime_error);
  }
}

value reverse(std::span<value const> args)
{
  if(args.size() != 1)
  {
    throw std::runtime_error("reverse(value) expects exactly 1 argument");
  }
  auto& arg = args[0];
  return reflex::visit(
      [](auto&& v) -> value {
        using U = std::decay_t<decltype(v)>;
        if constexpr(std::same_as<U, array>)
        {
          return v | std::views::reverse | std::ranges::to<array>();
        }
        else
        {
          throw runtime_error("Value of type {} is not an array", display_string_of(dealias(^^U)));
        }
      },
      arg);
}

TEST_CASE("reflex::jinja: pipes within expressions")
{
  jinja::basic_context ctx;
  ctx.def("reverse", reverse);

  SUBCASE("for loop - reverse")
  {
    auto tmpl = jinja::parse(
        "{% for item in items | reverse() %}"
        "{% if not loop.first %}, {% endif %}{{ loop.index }}: {{item}}"
        "{% endfor %}");

    ctx.set("items", array{"banana"s, "apple"s, "cherry"s});

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == R"(1: cherry, 2: apple, 3: banana)");
  }
  SUBCASE("for loop - reverse - nested item")
  {
    auto tmpl = jinja::parse(
        "{% for item in root.items | reverse() %}"
        "{% if not loop.first %}, {% endif %}{{ loop.index }}: {{item}}"
        "{% endfor %}");

    ctx.set(
        "root", object{
                    {"items", array{"banana"s, "apple"s, "cherry"s}}
    });

    auto result = jinja::render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == R"(1: cherry, 2: apple, 3: banana)");
  }
}
