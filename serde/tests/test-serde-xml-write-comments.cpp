#include <doctest/doctest.h>

import reflex.serde.xml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

struct xw_srv
{
  [[= xml::attribute]] int  port;
  [[= xml::attribute]] bool tls;
  std::string               host;
  int                       timeout;

  constexpr bool operator==(xw_srv const&) const = default;
};

struct xw_config
{
  std::string              name;
  int                      level;
  xw_srv                   srv;
  std::vector<std::string> tag;

  constexpr bool operator==(xw_config const&) const = default;
};

struct xw_small
{
  int                a;
  std::optional<int> b;

  constexpr bool operator==(xw_small const&) const = default;
};

struct xw_opt_only
{
  std::optional<int> x;

  constexpr bool operator==(xw_opt_only const&) const = default;
};

struct xw_user
{
  int v;

  constexpr bool operator==(xw_user const&) const = default;
};

template <typename OutputIt, auto Opts>
auto tag_invoke(
    tag_t<serde::serialize>,
    serde::xml::serializer<OutputIt, Opts>& ser,
    xw_user const&                          value) -> OutputIt
{
  return std::format_to(ser.out(), "<{0} v=\"{1}\"/>", ser.element_name("xw_user"), value.v);
}

template <typename InputIt, auto Opts>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::xml::deserializer<InputIt, Opts>& de,
    std::type_identity<xw_user>)
{
  de.read_open_tag();
  return xw_user{std::stoi(std::string{de.attributes().front().second})};
}

struct xw_with_user
{
  xw_user u;
  int     b;

  constexpr bool operator==(xw_with_user const&) const = default;
};

namespace
{
constexpr char config_data[] = {
#embed "fixtures/comments/xml/config.xml"
};
constexpr char expected_edit_data[] = {
#embed "fixtures/comments/xml/expected-edit.xml"
};
constexpr char config4_data[] = {
#embed "fixtures/comments/xml/config-indent4.xml"
};
constexpr char expected_edit4_data[] = {
#embed "fixtures/comments/xml/expected-edit-indent4.xml"
};

constexpr std::string_view config_xml{config_data, sizeof config_data};
constexpr std::string_view expected_edit_xml{expected_edit_data, sizeof expected_edit_data};
constexpr std::string_view config4_xml{config4_data, sizeof config4_data};
constexpr std::string_view expected_edit4_xml{expected_edit4_data, sizeof expected_edit4_data};

constexpr xml::read_options capture{.comments = true};
constexpr xml::options      write_pretty{.indent = 2, .comments = true};
constexpr xml::options      write_compact{.comments = true};

template <typename T> serde::commented<T> load(std::string_view in)
{
  xml::deserializer de{in, std::cw<capture>};
  return de.template load_commented<T>();
}

template <auto Opts, typename T>
std::string dump(T const& value, serde::comments_of<T> const& comments)
{
  std::string     out;
  xml::serializer ser{out, std::cw<Opts>};
  ser.dump(value, comments);
  return out;
}

using lines = std::vector<std::string>;
} // namespace

TEST_CASE("reflex::serde::xml write comments: config edit changes one line")
{
  auto [cfg, comments] = load<xw_config>(config_xml);
  CHECK_EQ(
      cfg, xw_config{
               "demo", 3, {8080, true, "localhost", 30},
                 {"a", "b"}
  });
  cfg.srv.host = "example.org";
  CHECK_EQ(dump<write_pretty>(cfg, comments), expected_edit_xml);
}

TEST_CASE("reflex::serde::xml write comments: an unchanged config is byte-identical")
{
  const auto [cfg, comments] = load<xw_config>(config_xml);
  const std::string out      = dump<write_pretty>(cfg, comments);
  CHECK_EQ(out, config_xml);
  const auto again = load<xw_config>(out);
  CHECK_EQ(again.value, cfg);
  CHECK_EQ(dump<write_pretty>(again.value, again.comments), config_xml);
}

TEST_CASE("reflex::serde::xml write comments: a 4-space config stays 4-space")
{
  auto [cfg, comments] = load<xw_config>(config4_xml);
  CHECK_EQ(serde::layout_of(comments).indent, 4);
  CHECK_EQ(dump<write_pretty>(cfg, comments), config4_xml);
  cfg.srv.host = "example.org";
  CHECK_EQ(dump<write_pretty>(cfg, comments), expected_edit4_xml);
}

TEST_CASE("reflex::serde::xml write comments: tabs stay tabs")
{
  constexpr std::string_view in = "<xw_small>\n\t<!-- x -->\n\t<a>1</a>\n</xw_small>\n";
  const auto [v, c]             = load<xw_small>(in);
  CHECK_EQ(dump<write_pretty>(v, c), in);
}

TEST_CASE("reflex::serde::xml write comments: a hand-built tree")
{
  serde::comments_of<xw_small> c;
  serde::comment(c).leading = {"top"};
  c.a.leading               = {"first", std::string{serde::blank_line}, "second"};
  c.a.trailing              = "one";
  c.b.leading               = {"absent, dropped"};
  serde::comment(c).footer  = {"bottom"};
  const std::string out     = dump<write_pretty>(xw_small{1, std::nullopt}, c);
  CHECK_EQ(
      out, "<!-- top -->\n"
           "<xw_small>\n"
           "  <!-- first -->\n"
           "\n"
           "  <!-- second -->\n"
           "  <a>1</a> <!-- one -->\n"
           "  <!-- bottom -->\n"
           "</xw_small>\n"sv);
}

TEST_CASE("reflex::serde::xml write comments: compact output keeps comments inline")
{
  serde::comments_of<xw_small> c;
  serde::layout_of(c)        = {0, ' ', false};
  serde::comment(c).leading  = {"top", std::string{serde::blank_line}};
  serde::comment(c).trailing = "self";
  c.a.leading                = {"lead"};
  c.a.trailing               = "trail";
  serde::comment(c).footer   = {"foot"};
  const xw_small    value{1, 2};
  const std::string out = dump<write_compact>(value, c);
  CHECK_EQ(
      out, "<!-- top --><xw_small> <!-- self --><!-- lead --><a>1</a> <!-- trail --><b>2</b>"
           "<!-- foot --></xw_small>"sv);
  CHECK_EQ(xml::deserializer{out}.load<xw_small>(), value);
}

TEST_CASE("reflex::serde::xml write comments: an empty aggregate puts its trailing after the close")
{
  serde::comments_of<xw_opt_only> c;
  serde::layout_of(c)        = {0, ' ', false};
  serde::comment(c).trailing = "t";
  CHECK_EQ(dump<write_pretty>(xw_opt_only{}, c), "<xw_opt_only></xw_opt_only> <!-- t -->"sv);
  CHECK_EQ(
      dump<write_pretty>(xw_opt_only{5}, c),
      "<xw_opt_only> <!-- t -->\n  <x>5</x>\n</xw_opt_only>"sv);
}

TEST_CASE("reflex::serde::xml write comments: a user override keeps its own comments")
{
  serde::comments_of<xw_with_user> c;
  serde::layout_of(c)          = {0, ' ', false};
  serde::comment(c.u).leading  = {"lead"};
  serde::comment(c.u).trailing = "trail";
  CHECK_EQ(
      dump<write_pretty>(xw_with_user{{7}, 2}, c),
      "<xw_with_user>\n  <!-- lead -->\n  <u v=\"7\"/> <!-- trail -->\n  <b>2</b>\n</xw_with_user>"sv);
}

TEST_CASE("reflex::serde::xml write comments: a scalar root")
{
  serde::comment_block c{{"h"}, "t", {"f"}};
  CHECK_EQ(dump<write_pretty>(4, c), "<!-- h -->\n<int>4</int> <!-- t -->\n<!-- f -->"sv);
}

TEST_CASE("reflex::serde::xml write comments: comment text the format cannot hold throws")
{
  serde::comments_of<xw_small> c;
  c.a.leading = {"a -- b"};
  CHECK_THROWS_WITH(
      dump<write_pretty>(xw_small{1, 2}, c), "XML: a comment cannot hold '--': 'a -- b'");
  c.a.leading = {"a\nb"};
  CHECK_THROWS_WITH(
      dump<write_pretty>(xw_small{1, 2}, c), "XML: a comment cannot hold a line break: 'a\nb'");
}

TEST_CASE("reflex::serde::xml write comments: a comment ending in - round-trips")
{
  serde::comments_of<xw_small> c;
  c.a.leading           = {"dash-"};
  c.a.trailing          = "-";
  const std::string out = dump<write_pretty>(xw_small{1, 2}, c);
  CHECK_EQ(out, "<xw_small>\n  <!-- dash- -->\n  <a>1</a> <!-- - -->\n  <b>2</b>\n</xw_small>\n"sv);
  const auto back = load<xw_small>(out);
  CHECK_EQ(back.comments.a.leading, lines{"dash-"});
  CHECK_EQ(back.comments.a.trailing, "-");
}

TEST_CASE("reflex::serde::xml write comments: a plain dump on a commenting serializer stays plain")
{
  std::string out;
  xml::serializer{out, std::cw<write_compact>}.dump(xw_small{1, 2});
  CHECK_EQ(out, "<xw_small><a>1</a><b>2</b></xw_small>"sv);
  std::string pretty;
  xml::serializer{pretty, std::cw<write_pretty>}.dump(xw_small{1, 2});
  CHECK_EQ(pretty, "<xw_small>\n  <a>1</a>\n  <b>2</b>\n</xw_small>"sv);
}
