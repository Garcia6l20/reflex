#include <doctest/doctest.h>

import reflex.serde.toml;

import std;

using namespace reflex;
using namespace reflex::serde;

using ta_lines = std::vector<std::string>;

struct ta_tls
{
  bool        on = false;
  std::string cert;

  constexpr bool operator==(ta_tls const&) const = default;
};

struct ta_server
{
  int         port = 0;
  std::string host;
  ta_tls      tls;

  constexpr bool operator==(ta_server const&) const = default;
};

struct ta_point
{
  int x = 0;
  int y = 0;

  constexpr bool operator==(ta_point const&) const = default;
};

struct ta_item
{
  std::string name;

  constexpr bool operator==(ta_item const&) const = default;
};

struct ta_config
{
  std::string                name;
  int                        workers = 0;
  ta_point                   origin;
  std::vector<int>           list;
  std::string                text;
  ta_server                  srv;
  std::map<std::string, int> limits;
  std::vector<ta_item>       items;

  constexpr bool operator==(ta_config const&) const = default;
};

struct ta_flat
{
  int a = 0;
  int b = 0;

  constexpr bool operator==(ta_flat const&) const = default;
};

struct ta_dotted_leaf
{
  int c = 0;

  constexpr bool operator==(ta_dotted_leaf const&) const = default;
};

struct ta_dotted_mid
{
  ta_dotted_leaf b;

  constexpr bool operator==(ta_dotted_mid const&) const = default;
};

struct ta_dotted
{
  ta_dotted_mid a;

  constexpr bool operator==(ta_dotted const&) const = default;
};

struct ta_section
{
  std::string host;
  int         port = 0;

  constexpr bool operator==(ta_section const&) const = default;
};

struct ta_sectioned
{
  ta_section srv;

  constexpr bool operator==(ta_sectioned const&) const = default;
};

struct ta_opt
{
  int                       a = 0;
  std::optional<ta_section> backup;

  constexpr bool operator==(ta_opt const&) const = default;
};

static constexpr toml::read_options ta_read{.comments = true};

template <typename T> static commented<T> ta_load(std::string const& text)
{
  toml::deserializer de{text, std::cw<ta_read>};
  return de.template load_commented<T>();
}

static constexpr std::string_view ta_document = R"(# header line
# second header line

# leading of name
name = "app" # trailing of name

workers = 4
# leading of origin
origin = { x = 1, y = 2 } # trailing of origin
list = [
  # inside an array
  1, # after an element
  2,
] # trailing of list
text = """
# not a comment
""" # trailing of text

# leading of srv
[srv] # trailing of srv
# leading of port
port = 8080 # trailing of port

# leading of srv.tls
[srv.tls] # trailing of srv.tls
on = true # trailing of on
cert = "c.pem"
# after the last key of srv.tls

# leading of limits
[limits] # trailing of limits
# leading of a map entry
cpu = 2 # trailing of a map entry

# before an array of tables
[[items]] # trailing of an element
# inside an element
name = "first" # trailing inside an element
[[items]]
name = "second"

# footer line
)";

TEST_CASE("reflex::serde::toml capture: every anchoring row")
{
  const std::string nl{blank_line};
  const auto [cfg, c] = ta_load<ta_config>(std::string{ta_document});

  SUBCASE("the value reads as without comments")
  {
    CHECK_EQ(cfg.name, "app");
    CHECK_EQ(cfg.workers, 4);
    CHECK(cfg.origin == ta_point{1, 2});
    CHECK(cfg.list == std::vector<int>{1, 2});
    CHECK_EQ(cfg.text, "# not a comment\n");
    CHECK_EQ(cfg.srv.port, 8080);
    CHECK(cfg.srv.tls.on);
    CHECK_EQ(cfg.srv.tls.cert, "c.pem");
    CHECK_EQ(cfg.limits.at("cpu"), 2);
    CHECK(cfg.items == std::vector<ta_item>{{"first"}, {"second"}});
  }

  SUBCASE("the document header ends at the last blank line before the first key")
  {
    CHECK(comment(c).leading == ta_lines{"header line", "second header line", nl});
    CHECK(c.name.leading == ta_lines{"leading of name"});
    CHECK(comment(c).trailing.empty());
  }

  SUBCASE("a leaf takes the lines above it and the comment on its line")
  {
    CHECK_EQ(c.name.trailing, "trailing of name");
    CHECK(c.workers.leading == ta_lines{nl});
    CHECK(c.workers.trailing.empty());
  }

  SUBCASE("an inline table anchors to the aggregate node's own block")
  {
    CHECK(comment(c.origin).leading == ta_lines{"leading of origin"});
    CHECK_EQ(comment(c.origin).trailing, "trailing of origin");
    CHECK(c.origin.x.leading.empty());
    CHECK(c.origin.x.trailing.empty());
  }

  SUBCASE("comments inside a multi-line array are dropped, after it is the trailing")
  {
    CHECK(c.list.leading.empty());
    CHECK_EQ(c.list.trailing, "trailing of list");
  }

  SUBCASE("a multi-line string body is content, after it is the trailing")
  {
    CHECK(c.text.leading.empty());
    CHECK_EQ(c.text.trailing, "trailing of text");
  }

  SUBCASE("a header takes the lines above it and the comment on its line")
  {
    CHECK(comment(c.srv).leading == ta_lines{nl, "leading of srv"});
    CHECK_EQ(comment(c.srv).trailing, "trailing of srv");
    CHECK(c.srv.port.leading == ta_lines{"leading of port"});
    CHECK_EQ(c.srv.port.trailing, "trailing of port");
  }

  SUBCASE("a key under a nested header walks to the nested leaf")
  {
    CHECK_EQ(c.srv.tls.on.trailing, "trailing of on");
    CHECK(c.srv.tls.on.leading.empty());
  }

  SUBCASE("a nested header anchors to the nested node")
  {
    CHECK(comment(c.srv.tls).leading == ta_lines{nl, "leading of srv.tls"});
    CHECK_EQ(comment(c.srv.tls).trailing, "trailing of srv.tls");
  }

  SUBCASE("a table has no footer: its last lines go to the next header")
  {
    CHECK(comment(c.srv.tls).footer.empty());
    CHECK(c.limits.leading == ta_lines{"after the last key of srv.tls", nl, "leading of limits"});
    CHECK_EQ(c.limits.trailing, "trailing of limits");
  }

  SUBCASE("a path through a map or an array of tables is dropped")
  {
    CHECK(c.items.leading.empty());
    CHECK(c.items.trailing.empty());
    CHECK(comment(c).footer == ta_lines{nl, "footer line"});
  }

  SUBCASE("the input ended with a line break, and TOML measures no indent")
  {
    CHECK(layout_of(c).final_newline);
    CHECK_EQ(layout_of(c).indent, 0);
  }
}

TEST_CASE("reflex::serde::toml capture: the section pattern before a key")
{
  const std::string nl{blank_line};
  const auto [v, c] =
      ta_load<ta_flat>("a = 1\n\n# Server settings\n\n# port to listen on\nb = 2\n");
  CHECK(v == ta_flat{1, 2});
  CHECK(c.b.leading == ta_lines{nl, "Server settings", nl, "port to listen on"});
  CHECK(comment(c).leading.empty());
}

TEST_CASE("reflex::serde::toml capture: the document header without a blank line")
{
  const auto [v, c] = ta_load<ta_flat>("# about a\na = 1\nb = 2");
  CHECK(v == ta_flat{1, 2});
  CHECK(comment(c).leading.empty());
  CHECK(c.a.leading == ta_lines{"about a"});
  CHECK_FALSE(layout_of(c).final_newline);
}

TEST_CASE("reflex::serde::toml capture: every blank line is one entry")
{
  const std::string nl{blank_line};
  const auto [v, c] = ta_load<ta_flat>("\n\n# top\n\n\na = 1\n\n\n\nb = 2\n\n");
  CHECK(v == ta_flat{1, 2});
  CHECK(comment(c).leading == ta_lines{nl, nl, "top", nl, nl});
  CHECK(c.a.leading.empty());
  CHECK(c.b.leading == ta_lines{nl, nl, nl});
  CHECK(comment(c).footer == ta_lines{nl});
  CHECK(layout_of(c).final_newline);
}

TEST_CASE("reflex::serde::toml capture: a bare comment marker is the empty string")
{
  const auto [v, c] = ta_load<ta_flat>("#\na = 1 #\nb = 2 #   spaced   \n");
  CHECK(c.a.leading == ta_lines{""});
  CHECK(c.a.trailing.empty());
  CHECK_EQ(c.b.trailing, "  spaced");
}

TEST_CASE("reflex::serde::toml capture: a dotted key under a header anchors to the leaf")
{
  const auto [v, c] = ta_load<ta_dotted>("[a] # on a\n# above b.c\nb.c = 1 # on b.c\n");
  CHECK_EQ(v.a.b.c, 1);
  CHECK_EQ(comment(c.a).trailing, "on a");
  CHECK(c.a.b.c.leading == ta_lines{"above b.c"});
  CHECK_EQ(c.a.b.c.trailing, "on b.c");
  CHECK(comment(c.a.b).leading.empty());
}

TEST_CASE("reflex::serde::toml capture: a header deeper than one level, no key between")
{
  const auto [v, c] = ta_load<ta_dotted>("# above\n[a.b] # on a.b\nc = 3\n");
  CHECK_EQ(v.a.b.c, 3);
  CHECK(comment(c.a.b).leading == ta_lines{"above"});
  CHECK_EQ(comment(c.a.b).trailing, "on a.b");
  CHECK(comment(c.a).leading.empty());
}

TEST_CASE("reflex::serde::toml capture: an optional inline table anchors to the node of its value")
{
  const auto [v, c] = ta_load<ta_opt>("a = 1\n# above backup\nbackup = { port = 2 } # on backup\n");
  REQUIRE(v.backup.has_value());
  CHECK_EQ(v.backup->port, 2);
  CHECK(comment(c.backup).leading == ta_lines{"above backup"});
  CHECK_EQ(comment(c.backup).trailing, "on backup");
}

TEST_CASE("reflex::serde::toml capture: a control character in a comment still throws")
{
  CHECK_THROWS_WITH(
      ta_load<ta_flat>("# bad \x01 byte\na = 1\n"),
      "TOML: a control character other than tab cannot appear in a comment");
  CHECK_THROWS_WITH(
      ta_load<ta_flat>("a = 1 # bad \x7f byte\n"),
      "TOML: a control character other than tab cannot appear in a comment");
  const auto [v, c] = ta_load<ta_flat>("a = 1 # a\ttab\n");
  CHECK_EQ(c.a.trailing, "a\ttab");
}

TEST_CASE("reflex::serde::toml capture: content after a value still throws")
{
  CHECK_THROWS_WITH(ta_load<ta_flat>("a = 1 2\n"), "TOML: unexpected content after a value");
}

TEST_CASE("reflex::serde::toml capture: comments around an array of tables are dropped")
{
  const std::string text = "# before\n[[items]] # on\n# inside\nname = \"x\" # after\n# footer\n";
  const auto [v, c]      = ta_load<ta_config>(text);
  CHECK(v.items == std::vector<ta_item>{{"x"}});
  CHECK(c.items.leading.empty());
  CHECK(c.items.trailing.empty());
  CHECK(comment(c).leading.empty());
  CHECK(comment(c).footer == ta_lines{"footer"});
}

TEST_CASE("reflex::serde::toml capture: an unknown key under skip drops its comments")
{
  constexpr toml::read_options opts{.comments = true, .unknown_keys = unknown_keys::skip};
  const std::string            text = "a = 1\n# above extra\nextra = 5 # on extra\nb = 2\n"
                                      "# above gone\n[gone] # on gone\nx = 1\n";
  toml::deserializer           de{text, std::cw<opts>};
  const auto [v, c] = de.load_commented<ta_flat>();
  CHECK(v == ta_flat{1, 2});
  CHECK(c.b.leading.empty());
  CHECK(c.b.trailing.empty());
  CHECK(comment(c).footer.empty());
}

TEST_CASE("reflex::serde::toml capture: CRLF line breaks")
{
  const std::string nl{blank_line};
  const auto [v, c] = ta_load<ta_flat>("# h\r\n\r\na = 1 # t\r\n\r\n# l\r\nb = 2\r\n");
  CHECK(v == ta_flat{1, 2});
  CHECK(comment(c).leading == ta_lines{"h", nl});
  CHECK_EQ(c.a.trailing, "t");
  CHECK(c.b.leading == ta_lines{nl, "l"});
  CHECK(layout_of(c).final_newline);
}

TEST_CASE("reflex::serde::toml capture: a plain load accepts the same document")
{
  const std::string  text = std::string{ta_document};
  toml::deserializer plain{text, std::cw<ta_read>};
  const auto         value = plain.load<ta_config>();
  CHECK(value == ta_load<ta_config>(text).value);
  toml::deserializer defaults{text};
  CHECK(defaults.load<ta_config>() == value);
}

TEST_CASE("reflex::serde::toml capture: a toml::value root reads, its comments are dropped")
{
  auto [v, c] = ta_load<toml::value>("# h\n\na = 1 # t\n# f\n");
  CHECK(v["a"] == 1);
  CHECK(c.leading.empty());
  CHECK(c.trailing.empty());
  CHECK(c.footer.empty());
}
