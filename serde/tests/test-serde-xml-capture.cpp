#include <doctest/doctest.h>

import reflex.serde.xml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

struct xa_srv
{
  [[= xml::attribute]] int port;
  std::string              host;

  constexpr bool operator==(xa_srv const&) const = default;
};

struct xa_config
{
  int              value;
  xa_srv           srv;
  std::vector<int> list;

  constexpr bool operator==(xa_config const&) const = default;
};

struct xa_small
{
  int a;
  int b;

  constexpr bool operator==(xa_small const&) const = default;
};

struct[[= serde::naming::camel_case]] xa_basic
{
  int                                    int_member;
  std::string                            string_member;
  [[= serde::naming::kebab_case]] double double_member;

  constexpr bool operator==(xa_basic const&) const = default;
};

struct xa_spaced
{
  [[= xml::attribute]] std::string lang;
  [[= xml::text]] std::string      body;

  constexpr bool operator==(xa_spaced const&) const = default;
};

struct xa_raw
{
  [[= xml::raw_content]] std::string inner;

  constexpr bool operator==(xa_raw const&) const = default;
};

struct xa_bodies
{
  std::string plain;
  xa_spaced   spaced;
  xa_raw      raw;

  constexpr bool operator==(xa_bodies const&) const = default;
};

struct xa_opts
{
  std::optional<xa_srv> backup;
  std::optional<int>    level;

  constexpr bool operator==(xa_opts const&) const = default;
};

struct xa_srvs
{
  std::vector<xa_srv> srv;

  constexpr bool operator==(xa_srvs const&) const = default;
};

struct xa_user
{
  int v;

  constexpr bool operator==(xa_user const&) const = default;
};

template <typename InputIt, auto Opts>
auto tag_invoke(
    tag_t<serde::deserialize>,
    serde::xml::deserializer<InputIt, Opts>& de,
    std::type_identity<xa_user>)
{
  auto [name, self_closing] = de.read_open_tag();
  if(not self_closing)
  {
    de.skip_subtree(false);
  }
  return xa_user{9};
}

struct xa_with_user
{
  xa_user u;
  int     b;

  constexpr bool operator==(xa_with_user const&) const = default;
};

namespace
{
constexpr xml::read_options capture{.comments = true};

template <typename T, typename In> serde::commented<T> load(In const& in)
{
  xml::deserializer de{in, std::cw<capture>};
  return de.template load_commented<T>();
}

using lines = std::vector<std::string>;

constexpr std::string_view full_doc = "<?xml version=\"1.0\"?>\n"
                                      "<!-- header -->\n"
                                      "\n"
                                      "<xa_config> <!-- config trailing -->\n"
                                      "  <!-- value leading -->\n"
                                      "  <value>1</value> <!-- value trailing -->\n"
                                      "\n"
                                      "  <!-- section -->\n"
                                      "  <srv port=\"8080\"> <!-- srv trailing -->\n"
                                      "    <host>a</host>\n"
                                      "    <!-- srv footer -->\n"
                                      "  </srv> <!-- after srv -->\n"
                                      "  <!-- list leading -->\n"
                                      "  <list>1</list> <!-- dropped same line -->\n"
                                      "  <!-- dropped -->\n"
                                      "  <list>2</list> <!-- list trailing -->\n"
                                      "  <!-- config footer -->\n"
                                      "</xa_config>\n"
                                      "<!-- after root -->\n";

void check_full(serde::commented<xa_config> const& r)
{
  CHECK_EQ(
      r.value, xa_config{
                   1, {8080, "a"},
                    {1,    2  }
  });
  auto const& c = r.comments;
  CHECK_EQ(serde::comment(c).leading, lines{"header", "\n"});
  CHECK_EQ(serde::comment(c).trailing, "config trailing");
  CHECK_EQ(c.value.leading, lines{"value leading"});
  CHECK_EQ(c.value.trailing, "value trailing");
  CHECK_EQ(serde::comment(c.srv).leading, lines{"\n", "section"});
  CHECK_EQ(serde::comment(c.srv).trailing, "srv trailing after srv");
  CHECK_EQ(serde::comment(c.srv).footer, lines{"srv footer"});
  CHECK_EQ(c.srv.host, serde::comment_block{});
  CHECK_EQ(c.srv.port, serde::comment_block{});
  CHECK_EQ(c.list.leading, lines{"list leading"});
  CHECK_EQ(c.list.trailing, "list trailing");
  CHECK_EQ(serde::comment(c).footer, lines{"config footer", "after root"});
  CHECK_EQ(serde::layout_of(c), serde::layout{2, ' ', true});
}
} // namespace

TEST_CASE("reflex::serde::xml capture: every anchoring row of an indented document")
{
  check_full(load<xa_config>(full_doc));
}

TEST_CASE("reflex::serde::xml capture: the streaming cursor anchors the same way")
{
  std::istringstream in{std::string{full_doc}};
  xml::deserializer  de{in, std::cw<capture>};
  check_full(de.load_commented<xa_config>());
}

TEST_CASE("reflex::serde::xml capture: a plain load with comments accepted")
{
  xml::deserializer de{full_doc, std::cw<capture>};
  CHECK_EQ(
      de.load<xa_config>(), xa_config{
                                1, {8080, "a"},
                                 {1,    2  }
  });
}

TEST_CASE("reflex::serde::xml capture: the body keeps - and >")
{
  const auto r =
      load<xa_small>("<xa_small>\n  <!-- a -> b -->\n  <a>1</a>\n  <b>2</b>\n</xa_small>"sv);
  CHECK_EQ(r.comments.a.leading, lines{"a -> b"});
}

TEST_CASE("reflex::serde::xml capture: a multi-line comment gives one entry per line")
{
  const auto r = load<xa_small>(
      "<xa_small>\n  <!-- first\n       second -->\n  <a>1</a>\n  <b>2</b> <!-- x\n y -->\n</xa_small>"sv);
  CHECK_EQ(r.comments.a.leading, lines{"first", "second"});
  CHECK_EQ(r.comments.b.trailing, "x");
  CHECK_EQ(serde::comment(r.comments).footer, lines{"y"});
}

TEST_CASE("reflex::serde::xml capture: an unknown element drops the comment above it")
{
  const std::string_view in = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                              "<!-- a comment -->\n"
                              "<Basic id=\"7\">\n"
                              "  <intMember>42</intMember>\n"
                              "  <!-- inline -->\n"
                              "  <extra><nested>ignored</nested><!-- inside --></extra>\n"
                              "  <stringMember>hello</stringMember>\n"
                              "  <double-member>3.14</double-member>\n"
                              "  <selfclosed/>\n"
                              "</Basic>";
  const auto             r  = load<xa_basic>(in);
  CHECK_EQ(r.value, xa_basic{42, "hello", 3.14});
  CHECK_EQ(serde::comment(r.comments).leading, lines{"a comment"});
  CHECK(r.comments.int_member.trailing.empty());
  CHECK(r.comments.string_member.leading.empty());
  CHECK(serde::comment(r.comments).footer.empty());
}

TEST_CASE("reflex::serde::xml capture: comments inside values are dropped")
{
  const auto r = load<xa_bodies>("<xa_bodies>\n"
                                 "  <plain>1<!-- x -->2</plain>\n"
                                 "  <spaced lang=\"en\"><!-- y -->text</spaced>\n"
                                 "  <raw><a/><!-- z --></raw>\n"
                                 "</xa_bodies>"sv);
  CHECK_EQ(r.value.plain, "12");
  CHECK_EQ(r.value.spaced.body, "text");
  CHECK_EQ(r.value.raw.inner, "<a/><!-- z -->");
  CHECK_EQ(r.comments.plain, serde::comment_block{});
  CHECK_EQ(serde::comment(r.comments.spaced), serde::comment_block{});
  CHECK_EQ(serde::comment(r.comments.raw), serde::comment_block{});
}

TEST_CASE("reflex::serde::xml capture: optional members")
{
  const auto r = load<xa_opts>("<xa_opts>\n"
                               "  <!-- b -->\n"
                               "  <backup port=\"1\"> <!-- bt -->\n"
                               "    <!-- h -->\n"
                               "    <host>x</host>\n"
                               "  </backup>\n"
                               "  <level>3</level> <!-- lt -->\n"
                               "</xa_opts>"sv);
  CHECK_EQ(
      r.value, xa_opts{
                   xa_srv{1, "x"},
                   3
  });
  CHECK_EQ(serde::comment(r.comments.backup).leading, lines{"b"});
  CHECK_EQ(serde::comment(r.comments.backup).trailing, "bt");
  CHECK_EQ(r.comments.backup.host.leading, lines{"h"});
  CHECK_EQ(r.comments.level.trailing, "lt");
}

TEST_CASE("reflex::serde::xml capture: a sequence of aggregates keeps the member's own comments")
{
  const auto r = load<xa_srvs>("<xa_srvs>\n"
                               "  <!-- first -->\n"
                               "  <srv port=\"1\"><!-- in --><host>a</host></srv>\n"
                               "  <!-- second -->\n"
                               "  <srv port=\"2\"><host>b</host></srv> <!-- last -->\n"
                               "</xa_srvs>"sv);
  CHECK_EQ(
      r.value, xa_srvs{
                   {{1, "a"}, {2, "b"}}
  });
  CHECK_EQ(r.comments.srv.leading, lines{"first"});
  CHECK_EQ(r.comments.srv.trailing, "last");
  CHECK(serde::comment(r.comments).footer.empty());
}

TEST_CASE("reflex::serde::xml capture: comments inside a user override element are dropped")
{
  const auto r = load<xa_with_user>(
      "<xa_with_user>\n  <!-- u -->\n  <u><!-- inside --><q/></u>\n  <b>2</b>\n</xa_with_user>"sv);
  CHECK_EQ(r.value, xa_with_user{{9}, 2});
  CHECK_EQ(serde::comment(r.comments.u).leading, lines{"u"});
  CHECK(serde::comment(r.comments.u).trailing.empty());
  CHECK(r.comments.b.leading.empty());
}

TEST_CASE("reflex::serde::xml capture: layout")
{
  SUBCASE("4 spaces")
  {
    const auto r = load<xa_small>("<xa_small>\n    <a>1</a>\n    <b>2</b>\n</xa_small>"sv);
    CHECK_EQ(serde::layout_of(r.comments), serde::layout{4, ' ', false});
  }
  SUBCASE("tabs")
  {
    const auto r = load<xa_small>("<xa_small>\n\t<a>1</a>\n\t<b>2</b>\n</xa_small>\n"sv);
    CHECK_EQ(serde::layout_of(r.comments), serde::layout{1, '\t', true});
  }
  SUBCASE("compact")
  {
    const auto r = load<xa_small>("<xa_small><!-- c --><a>1</a><b>2</b></xa_small>"sv);
    CHECK_EQ(serde::layout_of(r.comments), serde::layout{0, ' ', false});
    CHECK_EQ(serde::comment(r.comments).trailing, "c");
  }
}

TEST_CASE("reflex::serde::xml capture: a scalar root")
{
  const auto r = load<int>("<!-- h -->\n<int>4</int>\n<!-- f -->\n"sv);
  CHECK_EQ(r.value, 4);
  CHECK_EQ(r.comments.leading, lines{"h"});
  CHECK_EQ(r.comments.footer, lines{"f"});
}
