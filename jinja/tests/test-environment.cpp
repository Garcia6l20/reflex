#include <doctest/doctest.h>

import reflex.jinja;
import std;

using namespace reflex;
using namespace reflex::jinja;

using namespace std::string_literals;

using array = jinja::basic_context::array_type;

namespace
{
// Creates a unique temporary directory, removed on destruction.
struct temp_dir
{
  std::filesystem::path path;

  explicit temp_dir(std::string_view name)
      : path{std::filesystem::temp_directory_path() / std::format("reflex-jinja-{}", name)}
  {
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
  }

  ~temp_dir()
  {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }

  void write(std::string_view name, std::string_view content) const
  {
    std::ofstream out{path / name, std::ios::binary};
    out.write(content.data(), std::streamsize(content.size()));
  }
};
} // namespace

TEST_CASE("reflex::jinja: environment")
{
  jinja::basic_context ctx;

  SUBCASE("renders a named template and caches the parsed tree")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"a", "hello {{ who }}"},
                           {"b", "bye {{ who }}"},
                           }
        )
    };

    ctx.set("who", "reflex"s);

    CHECK(env.render("a", ctx) == "hello reflex");
    CHECK(env.render("b", ctx) == "bye reflex");
    CHECK(&env.get("a") == &env.get("a"));
  }

  SUBCASE("loads templates from the filesystem")
  {
    temp_dir dir{"filesystem-loader"};
    dir.write("foo.jinja", "x={{ x }}");

    jinja::environment env{jinja::filesystem_loader(dir.path)};

    ctx.set("x", 42);
    CHECK(env.has("foo"));
    CHECK(env.render("foo", ctx) == "x=42");
    // the extension may also be spelled out
    CHECK(env.render("foo.jinja", ctx) == "x=42");
  }

  SUBCASE("the filesystem loader refuses names escaping its root")
  {
    temp_dir dir{"filesystem-escape"};
    std::filesystem::create_directories(dir.path / "sub");
    dir.write("secret.jinja", "SECRET");

    jinja::environment env{jinja::filesystem_loader(dir.path / "sub")};

    CHECK(not env.has("../secret"));
    CHECK_THROWS(env.get("../secret"));

    auto absolute = (dir.path / "secret.jinja").string();
    CHECK(not env.has(absolute));
    CHECK_THROWS(env.get(absolute));
  }

  SUBCASE("a name merely beginning with dots is not an escape")
  {
    temp_dir dir{"filesystem-dotted"};
    dir.write("..hidden.jinja", "HIDDEN");

    jinja::environment env{jinja::filesystem_loader(dir.path)};

    CHECK(env.has("..hidden"));
    CHECK(env.render("..hidden", ctx) == "HIDDEN");
  }

  SUBCASE("a failed parse does not poison the cache")
  {
    temp_dir dir{"reload-after-parse-error"};
    dir.write("broken.jinja", "{% if x %}");

    jinja::environment env{jinja::filesystem_loader(dir.path)};

    CHECK_THROWS(env.get("broken"));

    dir.write("broken.jinja", "fixed");
    CHECK(env.render("broken", ctx) == "fixed");
  }

  SUBCASE("a missing template is not found and throws on get")
  {
    jinja::environment env{jinja::map_loader({})};

    CHECK(not env.has("nope"));
    CHECK_THROWS(env.get("nope"));
  }

  SUBCASE("the environment owns the source lifetime")
  {
    auto source = std::make_unique<std::string>("hello {{ who }}");

    jinja::environment env{
        jinja::map_loader({
                           {"a", *source},
                           }
        )
    };
    (void)env.get("a");
    source.reset();

    ctx.set("who", "reflex"s);
    CHECK(env.render("a", ctx) == "hello reflex");
  }

  SUBCASE("render_source owns its copy")
  {
    jinja::environment env{jinja::map_loader({})};

    ctx.set("who", "reflex"s);

    std::string result;
    {
      auto source = "hello {{ who }}"s;
      result      = env.render_source(source, ctx);
    }
    CHECK(result == "hello reflex");
  }
}

TEST_CASE("reflex::jinja: include")
{
  jinja::basic_context ctx;

  SUBCASE("includes share the context")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"header", "// {{ title }}"},
                           {"body", "{% include \"header\" %}\nx={{ x }}"},
                           }
        )
    };

    ctx.set("title", "generated"s);
    ctx.set("x", 42);

    CHECK(env.render("body", ctx) == "// generated\nx=42");
  }

  SUBCASE("includes see loop locals")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"item", "[{{ loop.index }}:{{ it }}]"},
                           {"list", "{% for it in items %}{% include 'item' %}{% endfor %}"},
                           }
        )
    };

    ctx.set("items", array{"a"s, "b"s});

    CHECK(env.render("list", ctx) == "[1:a][2:b]");
  }

  SUBCASE("a missing include throws")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"a", "{% include 'nope' %}"},
                           }
        )
    };

    CHECK_THROWS(env.render("a", ctx));
  }

  SUBCASE("a cyclic include throws")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"a", "{% include 'b' %}"},
                           {"b", "{% include 'a' %}"},
                           }
        )
    };

    CHECK_THROWS(env.render("a", ctx));
  }

  SUBCASE("include without an environment throws")
  {
    auto tmpl = jinja::parse("{% include 'a' %}");
    CHECK_THROWS(jinja::render(tmpl, ctx));
  }

  SUBCASE("an unquoted include name throws at parse time")
  {
    CHECK_THROWS(jinja::parse("{% include a %}"));
    CHECK_THROWS(jinja::parse("{% include %}"));
  }

  SUBCASE("include honours whitespace control on both sides")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"frag", "F"},
                           }
        )
    };

    CHECK(env.render_source("X{% include 'frag' -%}   \n   Y", ctx) == "XFY");
    CHECK(env.render_source("X   \n   {%- include 'frag' %}Y", ctx) == "XFY");
  }
}

TEST_CASE("reflex::jinja: extends / block")
{
  jinja::basic_context ctx;

  SUBCASE("a child overrides a base block")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"base", "[{% block body %}default{% endblock %}]"},
                           {"child", "{% extends \"base\" %}{% block body %}hi {{ x }}{% endblock %}"},
                           }
        )
    };

    ctx.set("x", "there"s);

    CHECK(env.render("base", ctx) == "[default]");
    CHECK(env.render("child", ctx) == "[hi there]");
  }

  SUBCASE("an unoverridden block keeps its default")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"base", "[{% block head %}H{% endblock %}|{% block body %}B{% endblock %}]"},
                           {"child", "{% extends 'base' %}{% block body %}b{% endblock %}"},
                           }
        )
    };

    CHECK(env.render("child", ctx) == "[H|b]");
  }

  SUBCASE("the most-derived override wins in a multi-level chain")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"c", "[{% block a %}ca{% endblock %}{% block b %}cb{% endblock %}]"},
                           {"b", "{% extends 'c' %}{% block a %}ba{% endblock %}{% block b %}bb{% endblock %}"},
                           {"a", "{% extends 'b' %}{% block a %}aa{% endblock %}"},
                           }
        )
    };

    CHECK(env.render("a", ctx) == "[aabb]");
  }

  SUBCASE("blocks nested in control flow are overridable")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"base", "{% for i in items %}{% block row %}-{{ i }}{% endblock %}{% endfor %}"},
                           {"child", "{% extends 'base' %}{% block row %}[{{ i }}]{% endblock %}"},
                           }
        )
    };

    ctx.set("items", array{"a"s, "b"s});

    CHECK(env.render("child", ctx) == "[a][b]");
  }

  SUBCASE("an include does not inherit the includer's block overrides")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"frag", "{% block body %}frag{% endblock %}"},
                           {"base", "[{% block body %}default{% endblock %}]{% include 'frag' %}"},
                           {"child", "{% extends 'base' %}{% block body %}child{% endblock %}"},
                           }
        )
    };

    CHECK(env.render("child", ctx) == "[child]frag");
  }

  SUBCASE("a cyclic inheritance chain throws")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"a", "{% extends 'b' %}"},
                           {"b", "{% extends 'a' %}"},
                           }
        )
    };

    CHECK_THROWS(env.render("a", ctx));
  }

  SUBCASE("extends must be the first meaningful tag")
  {
    CHECK_THROWS(jinja::parse("x{% extends 'base' %}"));
    CHECK_THROWS(jinja::parse("{% if x %}{% endif %}{% extends 'base' %}"));
  }

  SUBCASE("a leading comment does not hide extends")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"base", "[{% block body %}default{% endblock %}]"},
                           {"child", "{# header #}{% extends 'base' %}{% block body %}c{% endblock %}"},
                           }
        )
    };

    CHECK(env.render("child", ctx) == "[c]");
  }

  SUBCASE("malformed block tags throw")
  {
    CHECK_THROWS(jinja::parse("{% block %}x{% endblock %}"));
    CHECK_THROWS(jinja::parse("{% block a %}x"));
    CHECK_THROWS(jinja::parse("{% block a %}x{% endblock %}{% block a %}y{% endblock %}"));
  }

  SUBCASE("cloning a template re-indexes its blocks")
  {
    auto tmpl  = jinja::parse("[{% block body %}{{ x }}{% endblock %}]");
    auto clone = tmpl.clone();

    REQUIRE(clone.blocks.size() == 1);
    CHECK(clone.blocks[0].first == "body");
    CHECK(clone.blocks[0].second != tmpl.blocks[0].second);

    ctx.set("x", 7);
    CHECK(jinja::render(clone, ctx) == "[7]");
  }

  SUBCASE("moving a template keeps its block index valid")
  {
    auto  tmpl  = jinja::parse("[{% block body %}{{ x }}{% endblock %}]");
    auto* body  = tmpl.blocks.at(0).second;
    auto  moved = std::move(tmpl);

    // the children buffer is not reallocated by the move, so the index still points at it
    REQUIRE(moved.blocks.size() == 1);
    CHECK(moved.blocks[0].second == body);

    ctx.set("x", 7);
    CHECK(jinja::render(moved, ctx) == "[7]");
  }

  SUBCASE("endblock may repeat the block name")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"base", "[{% block body %}d{% endblock body %}]"},
                           {"child", "{% extends 'base' %}{% block body %}c{% endblock body %}"},
                           }
        )
    };

    CHECK(env.render("child", ctx) == "[c]");
    CHECK_THROWS(jinja::parse("{% block a %}x{% endblock b %}"));
  }

  SUBCASE("malformed names throw at parse time")
  {
    CHECK_THROWS(jinja::parse("{% block a scoped %}x{% endblock %}"));
    CHECK_THROWS(jinja::parse("{% include '' %}"));
    CHECK_THROWS(jinja::parse("{% extends '' %}"));
  }

  SUBCASE("blocks render standalone without an environment")
  {
    auto tmpl = jinja::parse("[{% block body %}{{ x }}{% endblock %}]");
    ctx.set("x", 7);
    CHECK(jinja::render(tmpl, ctx) == "[7]");
  }

  SUBCASE("extends without an environment throws instead of rendering the child")
  {
    auto tmpl = jinja::parse("{% extends 'base' %}{% block body %}c{% endblock %}");
    CHECK_THROWS(jinja::render(tmpl, ctx));
  }

  SUBCASE("an included template may extend another one")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"base", "[{% block body %}default{% endblock %}]"},
                           {"frag", "{% extends 'base' %}{% block body %}frag{% endblock %}"},
                           {"page", "<{% include 'frag' %}>"},
                           }
        )
    };

    CHECK(env.render("page", ctx) == "<[frag]>");
  }

  SUBCASE("a block is a scope of its own")
  {
    auto tmpl = jinja::parse("{% block body %}{% set x = 1 %}{{ x }}{% endblock %}[{{ x }}]");
    CHECK(jinja::render(tmpl, ctx) == "1[null]");
  }

  SUBCASE("a block scope does not hide the enclosing bindings")
  {
    auto tmpl = jinja::parse("{% set x = 1 %}{% block body %}{{ x }}{% endblock %}");
    CHECK(jinja::render(tmpl, ctx) == "1");
  }

  SUBCASE("a set in an overriding block dies with it")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"base", "[{% block body %}b{% endblock %}]{{ x }}"},
                           {"child", "{% extends 'base' %}{% block body %}{% set x = 2 %}{{ x }}{% endblock %}"},
                           }
        )
    };

    CHECK(env.render("child", ctx) == "[2]null");
  }

  SUBCASE("a grandchild overrides a block nested in another block")
  {
    jinja::environment env{
        jinja::map_loader({
                           {"c", "[{% block outer %}({% block inner %}ci{% endblock %}){% endblock %}]"},
                           {"b", "{% extends 'c' %}"},
                           {"a", "{% extends 'b' %}{% block inner %}ai{% endblock %}"},
                           }
        )
    };

    CHECK(env.render("a", ctx) == "[(ai)]");
  }
}
