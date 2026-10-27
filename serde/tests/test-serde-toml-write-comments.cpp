#include <doctest/doctest.h>

import reflex.serde.toml;

import std;

using namespace reflex;
using namespace reflex::serde;

using tw_lines = std::vector<std::string>;

struct tw_tls
{
  bool        on = false;
  std::string cert;

  constexpr bool operator==(tw_tls const&) const = default;
};

struct tw_server
{
  int         port = 0;
  std::string host;
  tw_tls      tls;

  constexpr bool operator==(tw_server const&) const = default;
};

struct tw_log
{
  std::string level;

  constexpr bool operator==(tw_log const&) const = default;
};

struct tw_config
{
  std::string      name;
  int              workers = 0;
  double           ratio   = 0;
  bool             debug   = false;
  std::vector<int> ports;
  tw_server        srv;
  tw_log           log;

  constexpr bool operator==(tw_config const&) const = default;
};

struct tw_port
{
  int port = 0;

  constexpr bool operator==(tw_port const&) const = default;
};

struct tw_shape
{
  int     value = 0;
  tw_port srv;

  constexpr bool operator==(tw_shape const&) const = default;
};

struct tw_two
{
  int     value = 0;
  tw_port first;
  tw_port second;

  constexpr bool operator==(tw_two const&) const = default;
};

struct tw_point
{
  int x = 0;
  int y = 0;

  constexpr bool operator==(tw_point const&) const = default;
};

struct tw_nested
{
  std::vector<std::vector<tw_point>> grid;
  std::vector<tw_port>               items;
  std::optional<int>                 maybe;
  std::map<std::string, int>         limits;

  constexpr bool operator==(tw_nested const&) const = default;
};

static constexpr char tw_config_text[] = {
#embed "fixtures/comments/toml/config.toml"
};

static constexpr char tw_expected_edit_text[] = {
#embed "fixtures/comments/toml/expected-edit.toml"
};

template <std::size_t N> static std::string tw_text(char const (&bytes)[N])
{
  return std::string{bytes, N};
}

static constexpr toml::read_options tw_read{.comments = true};
static constexpr toml::options      tw_write{.comments = true};

template <typename T> static commented<T> tw_load(std::string const& text)
{
  toml::deserializer de{text, std::cw<tw_read>};
  return de.template load_commented<T>();
}

template <typename T> static std::string tw_dump(T const& value, comments_of<T> const& comments)
{
  std::string out;
  toml::serializer{out, std::cw<tw_write>}.dump(value, comments);
  return out;
}

template <typename T> static std::string tw_plain(T const& value)
{
  std::string out;
  toml::serializer{out}.dump(value);
  return out;
}

TEST_CASE("reflex::serde::toml write: the fixture and its edit differ by one line")
{
  const std::string input    = tw_text(tw_config_text);
  const std::string expected = tw_text(tw_expected_edit_text);
  auto              lines    = [](std::string const& s) {
    tw_lines out;
    for(auto part : std::views::split(s, '\n'))
    {
      out.emplace_back(std::string_view{part});
    }
    return out;
  };
  const tw_lines a = lines(input);
  const tw_lines b = lines(expected);
  REQUIRE_EQ(a.size(), b.size());
  int differing = 0;
  for(std::size_t i = 0; i < a.size(); ++i)
  {
    differing += a[i] != b[i] ? 1 : 0;
  }
  CHECK_EQ(differing, 1);
}

TEST_CASE("reflex::serde::toml write: the config edit changes one line")
{
  const std::string input    = tw_text(tw_config_text);
  const std::string expected = tw_text(tw_expected_edit_text);

  auto [cfg, comments] = tw_load<tw_config>(input);
  CHECK_EQ(cfg.srv.port, 8080);
  cfg.srv.port = 9090;
  CHECK_EQ(tw_dump(cfg, comments), expected);
}

TEST_CASE("reflex::serde::toml write: an unchanged round trip is byte-identical")
{
  const std::string nl{blank_line};
  const std::string input    = tw_text(tw_config_text);
  const auto [cfg, comments] = tw_load<tw_config>(input);
  CHECK(comment(comments.srv).leading == tw_lines{nl, "Server settings"});
  CHECK(comment(comments.log).leading == tw_lines{nl, "Logging", nl, "one of debug, info, warn"});
  CHECK(comment(comments).footer == tw_lines{"", nl, "end of configuration"});
  const std::string out = tw_dump(cfg, comments);
  CHECK_EQ(out, input);
  const auto [cfg_back, back] = tw_load<tw_config>(out);
  CHECK(cfg_back == cfg);
  CHECK(back == comments);
}

TEST_CASE("reflex::serde::toml write: a hand-built tree gives the documented shape")
{
  const std::string     nl{blank_line};
  comments_of<tw_shape> c;
  comment(c).leading      = {"header", nl};
  comment(c).footer       = {"footer of root"};
  c.value.leading         = {"leading of value"};
  c.value.trailing        = "trailing of value";
  comment(c.srv).leading  = {nl, "section comment"};
  comment(c.srv).trailing = "trailing of srv";
  const tw_shape    value{1, {8080}};
  const std::string out = tw_dump(value, c);
  CHECK_EQ(out, R"(# header

# leading of value
value = 1 # trailing of value

# section comment
[srv] # trailing of srv
port = 8080
# footer of root
)");
  const auto [back, back_comments] = tw_load<tw_shape>(out);
  CHECK(back == value);
  CHECK(back_comments == c);
}

TEST_CASE("reflex::serde::toml write: a table footer reads back on the next header")
{
  const std::string   nl{blank_line};
  comments_of<tw_two> c;
  comment(c.first).footer   = {"footer of first"};
  comment(c.second).footer  = {nl, "footer of second"};
  comment(c.second).leading = {"leading of second"};
  const tw_two      value{1, {2}, {3}};
  const std::string out = tw_dump(value, c);
  CHECK_EQ(out, R"(value = 1
[first]
port = 2
# footer of first
# leading of second
[second]
port = 3

# footer of second
)");
  const auto [back, back_comments] = tw_load<tw_two>(out);
  CHECK(back == value);
  CHECK(comment(back_comments.first).footer.empty());
  CHECK(comment(back_comments.second).footer.empty());
  CHECK(comment(back_comments.second).leading == tw_lines{"footer of first", "leading of second"});
  CHECK(comment(back_comments).footer == tw_lines{nl, "footer of second"});
  CHECK_EQ(tw_dump(back, back_comments), out);
}

TEST_CASE("reflex::serde::toml write: a blank line in the first key's leading joins the header")
{
  const std::string     nl{blank_line};
  comments_of<tw_shape> c;
  c.value.leading                  = {"a", nl, "b"};
  const std::string out            = tw_dump(tw_shape{1, {2}}, c);
  const auto [back, back_comments] = tw_load<tw_shape>(out);
  CHECK(comment(back_comments).leading == tw_lines{"a", nl});
  CHECK(back_comments.value.leading == tw_lines{"b"});
  CHECK_EQ(tw_dump(back, back_comments), out);
}

TEST_CASE("reflex::serde::toml write: a leading blank line opens the document")
{
  const std::string     nl{blank_line};
  comments_of<tw_shape> c;
  comment(c).leading = {nl};
  CHECK_EQ(tw_dump(tw_shape{1, {2}}, c), "\nvalue = 1\n[srv]\nport = 2\n");
}

TEST_CASE("reflex::serde::toml write: an empty tree writes the plain bytes and a final newline")
{
  const tw_config value{
      "n", 1, 0.5, true, {1, 2},
          {80, "h", {true, "c"}},
          {"warn"}
  };
  const std::string plain = tw_plain(value);
  CHECK_EQ(tw_dump(value, comments_of<tw_config>{}), plain + "\n");

  comments_of<tw_config> no_newline;
  layout_of(no_newline).final_newline = false;
  CHECK_EQ(tw_dump(value, no_newline), plain);

  std::string tagged;
  toml::serializer{tagged, std::cw<tw_write>}.dump(value);
  CHECK_EQ(tagged, plain);
}

TEST_CASE("reflex::serde::toml write: an empty comment is a bare marker")
{
  comments_of<tw_shape> c;
  c.value.leading         = {""};
  c.value.trailing        = "";
  comment(c.srv).trailing = "";
  CHECK_EQ(tw_dump(tw_shape{1, {2}}, c), "#\nvalue = 1\n[srv]\nport = 2\n");
}

TEST_CASE("reflex::serde::toml write: a comment the reader would reject throws")
{
  comments_of<tw_shape> c;
  c.value.leading = {"two\nlines"};
  CHECK_THROWS_WITH(
      tw_dump(tw_shape{}, c), "TOML: a comment cannot hold a line break: 'two\nlines'");
  comments_of<tw_shape> d;
  d.value.trailing = "bell\x07";
  CHECK_THROWS_WITH(
      tw_dump(tw_shape{}, d),
      "TOML: a comment cannot hold a control character other than tab: 'bell\x07'");
  comments_of<tw_shape> e;
  e.value.trailing = "a\ttab";
  CHECK_EQ(tw_dump(tw_shape{1, {2}}, e), "value = 1 # a\ttab\n[srv]\nport = 2\n");
}

TEST_CASE("reflex::serde::toml write: inline aggregates, arrays of tables and omitted keys")
{
  tw_nested value;
  value.grid   = {{{1, 2}}, {}};
  value.items  = {{1}, {2}};
  value.limits = {
      {"cpu", 2}
  };

  comments_of<tw_nested> c;
  c.grid.leading    = {"above grid"};
  c.grid.trailing   = "on grid";
  c.items.leading   = {"above items"};
  c.items.trailing  = "on items";
  c.maybe.leading   = {"above maybe"};
  c.limits.leading  = {"above limits"};
  c.limits.trailing = "on limits";

  const std::string out = tw_dump(value, c);
  CHECK_EQ(out, R"(# above grid
grid = [[{ x = 1, y = 2 }], []] # on grid
# above limits
[limits] # on limits
cpu = 2
[[items]]
port = 1
[[items]]
port = 2
)");
  const auto [back, back_comments] = tw_load<tw_nested>(out);
  CHECK(back == value);
  CHECK(back_comments.grid.leading == tw_lines{"above grid"});
  CHECK_EQ(back_comments.limits.trailing, "on limits");
  CHECK(back_comments.items.leading.empty());
}
