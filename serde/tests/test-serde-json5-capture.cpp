#include <doctest/doctest.h>

import reflex.serde.json;

import std;

using namespace reflex;
using namespace reflex::serde;

struct ja_inner
{
  int         port = 0;
  std::string host;

  constexpr bool operator==(ja_inner const&) const = default;
};

struct ja_opaque
{
  int n = 0;

  constexpr bool operator==(ja_opaque const&) const = default;
};

template <typename InputIt, auto Opts>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::json::deserializer<InputIt, Opts>& de,
    std::type_identity<ja_opaque>)
{
  const auto doc = de.template load<json::value>();
  return ja_opaque{static_cast<int>(doc.at("n")->template as<double>())};
}

struct ja_cfg
{
  int                     value = 0;
  ja_inner                srv;
  std::optional<ja_inner> backup;
  std::vector<int>        list;
  ja_opaque               opaque;

  constexpr bool operator==(ja_cfg const&) const = default;
};

constexpr json::read_options ja_opts{.comments = true};
constexpr json::read_options ja_skip_opts{.comments = true, .unknown_keys = unknown_keys::skip};

static commented<ja_cfg> ja_load(std::string const& text)
{
  json::deserializer de{text, std::cw<ja_opts>};
  return de.load_commented<ja_cfg>();
}

static commented<ja_cfg> ja_load_stream(std::string const& text)
{
  std::istringstream in{text};
  json::deserializer de{in, std::cw<ja_opts>};
  return de.load_commented<ja_cfg>();
}

using ja_lines = std::vector<std::string>;

static const std::string ja_blank{serde::blank_line};

TEST_CASE("reflex::serde::json5 capture: header, root trailing and footer")
{
  const std::string text = R"(// h1
/* h2 */
{ // rt
  "value": 1
  // f1
}
// after
)";
  for(auto const& [value, c] : {ja_load(text), ja_load_stream(text)})
  {
    CHECK_EQ(value.value, 1);
    CHECK_EQ(comment(c).leading, ja_lines{"h1", "h2"});
    CHECK_EQ(comment(c).trailing, "rt");
    CHECK_EQ(comment(c).footer, ja_lines{"f1", "after"});
    CHECK(c.value == comment_block{});
    CHECK(layout_of(c).final_newline);
  }
}

TEST_CASE("reflex::serde::json5 capture: leading comments of scalar and aggregate members")
{
  const auto [value, c] = ja_load(R"({
  // of value
  "value": 1,
  // of srv
  // second line
  "srv": {
    // of port
    "port": 8080,
    "host": "a"
  }
})");
  CHECK_EQ(value.srv, ja_inner{8080, "a"});
  CHECK_EQ(c.value.leading, ja_lines{"of value"});
  CHECK_EQ(comment(c.srv).leading, ja_lines{"of srv", "second line"});
  CHECK_EQ(c.srv.port.leading, ja_lines{"of port"});
  CHECK(c.srv.host == comment_block{});
}

TEST_CASE("reflex::serde::json5 capture: trailing comments")
{
  const auto [value, c] = ja_load(R"({
  "value": 1, // after comma
  "srv": { // srv own
    "port": 8080 /* before comma */, // after too
    "host": "a" // last member
  }, // srv after
  "list": [1]
})");
  CHECK_EQ(c.value.trailing, "after comma");
  CHECK_EQ(c.srv.port.trailing, "before comma after too");
  CHECK_EQ(c.srv.host.trailing, "last member");
  CHECK_EQ(comment(c.srv).trailing, "srv own srv after");
  CHECK(comment(c.srv).footer.empty());
  CHECK(c.list == comment_block{});
}

TEST_CASE("reflex::serde::json5 capture: blank lines")
{
  const auto [value, c] = ja_load(R"(
// header

{

  // a

  // b

  "value": 1,


  "srv": {"port": 1, "host": "h"}

}
)");
  CHECK_EQ(comment(c).leading, ja_lines{ja_blank, "header", ja_blank});
  CHECK_EQ(c.value.leading, ja_lines{ja_blank, "a", ja_blank, "b", ja_blank});
  CHECK_EQ(comment(c.srv).leading, ja_lines{ja_blank, ja_blank});
  CHECK_EQ(comment(c).footer, ja_lines{ja_blank});
}

TEST_CASE("reflex::serde::json5 capture: a multi-line block comment splits into lines")
{
  const auto [value, c] = ja_load(R"({
  /*
   * one
   * two
   */
  "value": 1 /* tail
  next */
})");
  CHECK_EQ(c.value.leading, ja_lines{"one", "two"});
  CHECK_EQ(c.value.trailing, "tail");
  CHECK_EQ(comment(c).footer, ja_lines{"next"});
}

TEST_CASE("reflex::serde::json5 capture: comments inside a sequence are dropped")
{
  const auto [value, c] = ja_load(R"({
  "list": [ // in
    1, // x
    // y
    2
  ],
  "value": 1
})");
  CHECK_EQ(value.list, std::vector<int>{1, 2});
  CHECK(c.list == comment_block{});
  CHECK(c.value == comment_block{});
}

TEST_CASE("reflex::serde::json5 capture: a user override keeps the comments around its key")
{
  const auto [value, c] = ja_load(R"({
  // above opaque
  "opaque": { // inside
    // inside too
    "n": 3
  }, // opaque trailing
  "value": 1
})");
  CHECK_EQ(value.opaque.n, 3);
  CHECK_EQ(comment(c.opaque).leading, ja_lines{"above opaque"});
  CHECK_EQ(comment(c.opaque).trailing, "opaque trailing");
  CHECK(c.opaque.n == comment_block{});
  CHECK(c.value == comment_block{});
}

TEST_CASE("reflex::serde::json5 capture: an engaged optional aggregate")
{
  const auto [value, c] = ja_load(R"({
  // b
  "backup": { // bt
    // p
    "port": 2, "host": "x"
  }
})");
  CHECK_EQ(
      value.backup, std::optional<ja_inner>{
                        ja_inner{2, "x"}
  });
  CHECK_EQ(comment(c.backup).leading, ja_lines{"b"});
  CHECK_EQ(comment(c.backup).trailing, "bt");
  CHECK_EQ(c.backup.port.leading, ja_lines{"p"});

  const auto [none, nc] = ja_load("{\n  // b\n  \"backup\": null, // n\n  \"value\": 1\n}");
  CHECK_FALSE(none.backup.has_value());
  CHECK_EQ(comment(nc.backup).leading, ja_lines{"b"});
  CHECK_EQ(comment(nc.backup).trailing, "n");
}

TEST_CASE("reflex::serde::json5 capture: layout")
{
  CHECK_EQ(layout_of(ja_load("{\n  \"value\": 1\n}\n").comments).indent, 2);
  CHECK_EQ(layout_of(ja_load("{\n    \"value\": 1\n}\n").comments).indent, 4);
  CHECK_EQ(layout_of(ja_load_stream("{\n    \"value\": 1\n}\n").comments).indent, 4);
  CHECK_EQ(layout_of(ja_load("// x\n{\n    // y\n    \"value\": 1\n}").comments).indent, 4);
  CHECK_EQ(layout_of(ja_load(R"({"value": 1})").comments).indent, 0);

  const auto tabbed = ja_load("{\n\t\"value\": 1\n}\n").comments;
  CHECK_EQ(layout_of(tabbed).indent, 1);
  CHECK_EQ(layout_of(tabbed).indent_char, '\t');

  CHECK(layout_of(ja_load("{\"value\": 1}\n").comments).final_newline);
  CHECK_FALSE(layout_of(ja_load("{\"value\": 1}").comments).final_newline);
  CHECK(layout_of(ja_load_stream("{\"value\": 1}\n").comments).final_newline);
  CHECK_FALSE(layout_of(ja_load_stream("{\"value\": 1}").comments).final_newline);
}

TEST_CASE("reflex::serde::json5 capture: an unknown key drops its comments under skip")
{
  const std::string  text = R"({
  // gone
  "extra": {"a": [1]}, // gone too
  // kept
  "value": 1
})";
  json::deserializer de{text, std::cw<ja_skip_opts>};
  const auto [value, c] = de.load_commented<ja_cfg>();
  CHECK_EQ(value.value, 1);
  CHECK_EQ(c.value.leading, ja_lines{"kept"});
  CHECK(comment(c).trailing.empty());
  CHECK(comment(c).footer.empty());
}

TEST_CASE("reflex::serde::json5 capture: a plain load reads the same value and captures nothing")
{
  const std::string  text = R"(// h
{ // t
  "value": 1, // v
  "srv": {"port": 1, "host": "h"} // s
  // f
}
)";
  json::deserializer de{text, std::cw<ja_opts>};
  const auto         plain = de.load<ja_cfg>();
  CHECK(plain == ja_load(text).value);
  CHECK(de.comments_.pending.empty());
  CHECK_FALSE(de.comments_.active);
}

TEST_CASE("reflex::serde::json5 capture: a scalar root")
{
  json::deserializer de{std::string_view{"// h\n42 // t\n"}, std::cw<ja_opts>};
  const auto [value, c] = de.load_commented<int>();
  CHECK_EQ(value, 42);
  CHECK_EQ(c.leading, ja_lines{"h"});
  CHECK_EQ(c.footer, ja_lines{"t"});
}
