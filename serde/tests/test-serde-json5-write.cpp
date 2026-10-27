#include <doctest/doctest.h>

import reflex.serde.json;

import std;

using namespace reflex;
using namespace reflex::serde;

struct jw_server
{
  int         port = 0;
  std::string host;
  bool        tls = false;

  constexpr bool operator==(jw_server const&) const = default;
};

struct jw_config
{
  std::string name;
  int         workers = 0;
  jw_server   srv;
  bool        debug = false;
  double      ratio = 0;

  constexpr bool operator==(jw_config const&) const = default;
};

struct jw_port
{
  int port = 0;

  constexpr bool operator==(jw_port const&) const = default;
};

struct jw_shape
{
  int     value = 0;
  jw_port srv;

  constexpr bool operator==(jw_shape const&) const = default;
};

struct jw_lists
{
  std::vector<jw_port>   ports;
  std::optional<jw_port> backup;
  int                    after = 0;

  constexpr bool operator==(jw_lists const&) const = default;
};

struct jw_empty
{};

static constexpr char jw_config_text[] = {
#embed "fixtures/comments/json/config.json5"
};

static constexpr char jw_expected_edit_text[] = {
#embed "fixtures/comments/json/expected-edit.json5"
};

static constexpr char jw_config_indent4_text[] = {
#embed "fixtures/comments/json/config-indent4.json5"
};

static constexpr char jw_expected_edit_indent4_text[] = {
#embed "fixtures/comments/json/expected-edit-indent4.json5"
};

template <std::size_t N> static std::string jw_text(char const (&bytes)[N])
{
  return std::string{bytes, N};
}

template <typename N> static bool jw_same(N const& a, N const& b)
{
  if constexpr(requires { a == b; })
  {
    return a == b;
  }
  else
  {
    bool same = true;
    template for(constexpr auto m : std::define_static_array(
                     nonstatic_data_members_of(^^N, std::meta::access_context::unchecked())))
    {
      same = same and jw_same(a.[:m:], b.[:m:]);
    }
    return same;
  }
}

constexpr json::read_options jw_read{.comments = true};
constexpr json::options      jw_write{.indent = 2, .comments = true};

template <typename T> static commented<T> jw_load(std::string const& text)
{
  json::deserializer de{text, std::cw<jw_read>};
  return de.template load_commented<T>();
}

template <typename T> static std::string jw_dump(T const& value, comments_of<T> const& comments)
{
  std::string out;
  json::serializer{out, std::cw<jw_write>}.dump(value, comments);
  return out;
}

TEST_CASE("reflex::serde::json5 write: the config edit changes one line")
{
  const std::string input    = jw_text(jw_config_text);
  const std::string expected = jw_text(jw_expected_edit_text);

  auto [cfg, comments] = jw_load<jw_config>(input);
  CHECK_EQ(cfg.srv.port, 8080);
  cfg.srv.port = 9090;
  CHECK_EQ(jw_dump(cfg, comments), expected);
}

TEST_CASE("reflex::serde::json5 write: an unchanged round trip is byte-identical")
{
  const std::string input    = jw_text(jw_config_text);
  const auto [cfg, comments] = jw_load<jw_config>(input);
  const std::string out      = jw_dump(cfg, comments);
  CHECK_EQ(out, input);
  const auto [cfg_back, back] = jw_load<jw_config>(out);
  CHECK(cfg_back == cfg);
  CHECK(jw_same(back, comments));
}

TEST_CASE("reflex::serde::json5 write: a 4-space input stays 4-space")
{
  const std::string input    = jw_text(jw_config_indent4_text);
  const std::string expected = jw_text(jw_expected_edit_indent4_text);

  auto [cfg, comments] = jw_load<jw_config>(input);
  CHECK_EQ(layout_of(comments).indent, 4);
  CHECK_EQ(jw_dump(cfg, comments), input);
  cfg.srv.port = 9090;
  CHECK_EQ(jw_dump(cfg, comments), expected);
}

TEST_CASE("reflex::serde::json5 write: a hand-built tree gives the documented shape")
{
  comments_of<jw_shape> c;
  comment(c).leading      = {"header line", std::string{blank_line}};
  comment(c).trailing     = "root trailing";
  comment(c).footer       = {"footer of root"};
  c.value.leading         = {"leading of value"};
  c.value.trailing        = "trailing of value";
  comment(c.srv).leading  = {std::string{blank_line}, "section comment"};
  comment(c.srv).trailing = "trailing of srv";
  comment(c.srv).footer   = {"footer of srv"};
  const jw_shape    value{1, {8080}};
  const std::string out = jw_dump(value, c);
  CHECK_EQ(out, R"(// header line

{ // root trailing
  // leading of value
  "value": 1, // trailing of value

  // section comment
  "srv": { // trailing of srv
    "port": 8080
    // footer of srv
  }
  // footer of root
}
)");
  const auto [back, back_comments] = jw_load<jw_shape>(out);
  CHECK(back == value);
  CHECK_EQ(layout_of(back_comments).indent, 2);
  layout_of(c).indent = 2;
  CHECK(jw_same(back_comments, c));
}

TEST_CASE("reflex::serde::json5 write: an empty tree writes the pretty bytes and a final newline")
{
  const jw_config value{
      "n", 1, {80, "h", true},
        false, 0.25
  };
  std::string pretty;
  json::serializer{pretty, std::cw<json::options{.indent = 2}>}.dump(value);
  CHECK_EQ(jw_dump(value, comments_of<jw_config>{}), pretty + "\n");

  comments_of<jw_config> no_newline;
  layout_of(no_newline).final_newline = false;
  CHECK_EQ(jw_dump(value, no_newline), pretty);
}

TEST_CASE("reflex::serde::json5 write: a comment holding a line break throws, naming JSON")
{
  comments_of<jw_shape> c;
  c.value.leading = {"two\nlines"};
  CHECK_THROWS_WITH(
      jw_dump(jw_shape{}, c), "JSON: a comment cannot hold a line break: 'two\nlines'");
  comments_of<jw_shape> t;
  t.value.trailing = "cr\r";
  CHECK_THROWS(jw_dump(jw_shape{}, t));
}

TEST_CASE("reflex::serde::json5 write: an empty aggregate keeps its trailing and footer")
{
  comments_of<jw_empty> c;
  comment(c).trailing = "t";
  comment(c).footer   = {"f"};
  const auto out      = jw_dump(jw_empty{}, c);
  CHECK_EQ(out, "{ // t\n  // f\n}\n");
  CHECK(jw_same(jw_load<jw_empty>(out).comments, c));
  CHECK_EQ(jw_dump(jw_empty{}, comments_of<jw_empty>{}), "{}\n");
}

TEST_CASE("reflex::serde::json5 write: a tab-indented input stays tab-indented")
{
  const std::string input = "{\n\t\"value\": 1, // v\n\t\"srv\": {\n\t\t\"port\": 2\n\t}\n}\n";
  const auto [value, c]   = jw_load<jw_shape>(input);
  CHECK_EQ(layout_of(c).indent_char, '\t');
  CHECK_EQ(jw_dump(value, c), input);
}

TEST_CASE("reflex::serde::json5 write: sequences and a null optional aggregate")
{
  const std::string input = R"({
  // ports
  "ports": [
    {
      "port": 1
    }
  ], // after ports
  // backup
  "backup": null, // none
  "after": 3
}
)";
  const auto [value, c]   = jw_load<jw_lists>(input);
  CHECK_EQ(comment(c.backup).trailing, "none");
  CHECK_EQ(jw_dump(value, c), input);
}

TEST_CASE("reflex::serde::json5 write: a scalar root")
{
  comment_block c;
  c.leading = {"h"};
  c.footer  = {"f"};
  CHECK_EQ(jw_dump(42, c), "// h\n42\n// f\n");
}

TEST_CASE("reflex::serde::json5 write: the serializer is reusable after a throw")
{
  comments_of<jw_shape> bad;
  bad.value.leading = {"a\nb"};
  std::string      out;
  json::serializer ser{out, std::cw<jw_write>};
  CHECK_THROWS(ser.dump(jw_shape{}, bad));
  CHECK_FALSE(ser.comments_.active);
  CHECK_EQ(ser.indent_.depth, 0);
  out.clear();
  ser.dump(jw_port{5});
  CHECK_EQ(out, "{\n  \"port\": 5\n}");
}
