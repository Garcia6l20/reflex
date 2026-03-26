#include <doctest/doctest.h>

#include <reflex/jinja.hpp>

#include <types.hpp>

using namespace reflex;
using namespace reflex::jinja;
using namespace reflex::serde;

using namespace reflex::literals;
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

TEST_CASE("reflex::jinja: aggregate support")
{
  SUBCASE("basic")
  {
    aggregate1 agg{42, "hello"s};
    auto       ctx = expr::context{"agg"_na = agg};

    auto tmpl   = parse("a={{ agg.a }}, b={{ agg.b }}");
    auto result = render(tmpl, ctx);
    CHECK(result == "a=42, b=hello");
  }

  SUBCASE("nested")
  {
    aggregate2 agg{
        3.14, {42, "world"s}
    };
    auto ctx = expr::context{"agg"_na = agg};

    auto tmpl   = parse("x={{ agg.x }}, a={{ agg.nested.a }}, b={{ agg.nested.b }}");
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

    auto tmpl = parse(
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

    auto tmpl = parse(
        "{% for item in agg.nested_list[0].nested_list %}a={{ item.a }}, b={{ item.b }}\n"
        "{% endfor %}");
    auto result = render(tmpl, ctx);
    std::println("{}", result);
    CHECK(result == "a=1, b=one\na=2, b=two\na=3, b=three\n");
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
