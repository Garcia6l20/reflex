#include <doctest/doctest.h>

import reflex.serde.json;
import reflex.serde.toml;
import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;

struct it_point
{
  int x = 0;
  int y = 0;

  constexpr bool operator==(it_point const&) const = default;
};

struct it_holder
{
  int                                a = 0;
  [[= serde::inline_table]] it_point p;
  it_point                           q;

  constexpr bool operator==(it_holder const&) const = default;
};

struct it_plain_holder
{
  int      a = 0;
  it_point p;
  it_point q;
};

struct it_late
{
  it_point                           q;
  [[= serde::inline_table]] it_point p;
  int                                a = 0;

  constexpr bool operator==(it_late const&) const = default;
};

struct it_sub
{
  int k = 0;

  constexpr bool operator==(it_sub const&) const = default;
};

struct it_nested_point
{
  int    x = 0;
  it_sub s;

  constexpr bool operator==(it_nested_point const&) const = default;
};

struct it_nested
{
  [[= serde::inline_table]] it_nested_point p;

  constexpr bool operator==(it_nested const&) const = default;
};

struct it_array
{
  [[= serde::inline_table]] std::vector<it_point> v;

  constexpr bool operator==(it_array const&) const = default;
};

struct it_array_plain
{
  std::vector<it_point> v;
};

struct it_map
{
  [[= serde::inline_table]] std::map<std::string, int> m;

  bool operator==(it_map const&) const = default;
};

struct it_optional
{
  [[= serde::inline_table]] std::optional<it_point> p;
  int                                               a = 0;

  constexpr bool operator==(it_optional const&) const = default;
};

struct it_omitted
{
  [[= serde::inline_table]][[= serde::omit_if_empty{}]] std::map<std::string, int> m;
  int                                                                              a = 0;

  bool operator==(it_omitted const&) const = default;
};

struct it_section
{
  int                                k = 0;
  [[= serde::inline_table]] it_point p;

  constexpr bool operator==(it_section const&) const = default;
};

struct it_row
{
  int                                id = 0;
  [[= serde::inline_table]] it_point p;

  constexpr bool operator==(it_row const&) const = default;
};

struct it_deep
{
  int                 a = 0;
  it_section          q;
  std::vector<it_row> rows;

  constexpr bool operator==(it_deep const&) const = default;
};

static_assert(serde::is_inline_table(^^it_holder::p));
static_assert(not serde::is_inline_table(^^it_holder::q));

static constexpr toml::read_options it_read_commented{.comments = true};
static constexpr toml::options      it_write_commented{.comments = true};

template <typename T> static std::string it_toml(T const& value)
{
  std::string out;
  toml::serializer{out}.dump(value);
  return out;
}

template <typename T> static T it_read(std::string const& text)
{
  return toml::deserializer{text}.template load<T>();
}

template <typename T> static std::string it_json(T const& value)
{
  std::string out;
  json::serializer{out}.dump(value);
  return out;
}

template <typename T> static std::string it_yaml(T const& value)
{
  std::string out;
  yaml::serializer{out}.dump(value);
  return out;
}

static it_holder it_holder_value()
{
  return {
      .a = 1, .p = {1, 2},
           .q = {3, 4}
  };
}

static it_plain_holder it_plain_holder_value()
{
  return {
      .a = 1, .p = {1, 2},
           .q = {3, 4}
  };
}

TEST_CASE("serde::inline_table: an annotated member is written inline, its sibling as a header")
{
  const auto value = it_holder_value();
  const auto text  = it_toml(value);
  CHECK_EQ(text, "a = 1\np = { x = 1, y = 2 }\n[q]\nx = 3\ny = 4");
  CHECK(it_read<it_holder>(text) == value);
  CHECK_EQ(it_toml(it_plain_holder_value()), "a = 1\n[p]\nx = 1\ny = 2\n[q]\nx = 3\ny = 4");
}

TEST_CASE("serde::inline_table: an inline member after a header member is written above it")
{
  const it_late value{
      .q = {3, 4},
        .p = {1, 2},
        .a = 5
  };
  const auto text = it_toml(value);
  CHECK_EQ(text, "p = { x = 1, y = 2 }\na = 5\n[q]\nx = 3\ny = 4");
  CHECK(it_read<it_late>(text) == value);
}

TEST_CASE("serde::inline_table: an inline member under a header and in an array of tables")
{
  const it_deep value{
      .a = 1, .q = {.k = 5,                 .p = {1, 2}           },
           .rows = {{.id = 1, .p = {2, 3}}, {.id = 2, .p = {4, 5}}}
  };
  const auto text = it_toml(value);
  CHECK_EQ(
      text,
      "a = 1\n[q]\nk = 5\np = { x = 1, y = 2 }\n[[rows]]\nid = 1\np = { x = 2, y = 3 }\n[[rows]]\n"
      "id = 2\np = { x = 4, y = 5 }");
  CHECK(it_read<it_deep>(text) == value);
}

TEST_CASE("serde::inline_table: a nested table stays on the one line")
{
  const it_nested value{
      .p = {.x = 1, .s = {.k = 2}}
  };
  const auto text = it_toml(value);
  CHECK_EQ(text, "p = { x = 1, s = { k = 2 } }");
  CHECK(it_read<it_nested>(text) == value);
}

TEST_CASE("serde::inline_table: an array of tables is written as an inline array")
{
  const it_array value{
      .v = {{1, 2}, {3, 4}}
  };
  const auto text = it_toml(value);
  CHECK_EQ(text, "v = [{ x = 1, y = 2 }, { x = 3, y = 4 }]");
  CHECK(it_read<it_array>(text) == value);
  CHECK_EQ(
      it_toml(
          it_array_plain{
              .v = {{1, 2}, {3, 4}}
  }),
      "[[v]]\nx = 1\ny = 2\n[[v]]\nx = 3\ny = 4");
}

TEST_CASE("serde::inline_table: a map member is written inline")
{
  const it_map value{
      .m = {{"a", 1}, {"b", 2}}
  };
  const auto text = it_toml(value);
  CHECK_EQ(text, "m = { a = 1, b = 2 }");
  CHECK(it_read<it_map>(text) == value);
}

TEST_CASE("serde::inline_table: an optional member is inline when engaged and absent when empty")
{
  const it_optional engaged{
      .p = it_point{1, 2},
        .a = 3
  };
  const auto text = it_toml(engaged);
  CHECK_EQ(text, "p = { x = 1, y = 2 }\na = 3");
  CHECK(it_read<it_optional>(text) == engaged);

  const it_optional empty{.p = std::nullopt, .a = 3};
  CHECK_EQ(it_toml(empty), "a = 3");
  CHECK(it_read<it_optional>("a = 3\n") == empty);
}

TEST_CASE("serde::inline_table: omit_if_empty still omits an empty member")
{
  CHECK_EQ(it_toml(it_omitted{.m = {}, .a = 1}), "a = 1");
  CHECK_EQ(it_toml(it_omitted{.m = {{"k", 2}}, .a = 1}), "m = { k = 2 }\na = 1");
}

TEST_CASE("serde::inline_table: a commented dump keeps the member's own comments")
{
  comments_of<it_holder> c;
  comment(c.p).leading  = {"leading of p"};
  comment(c.p).trailing = "trailing of p";
  comment(c.q).leading  = {"leading of q"};

  std::string out;
  toml::serializer{out, std::cw<it_write_commented>}.dump(it_holder_value(), c);
  CHECK_EQ(
      out,
      "a = 1\n# leading of p\np = { x = 1, y = 2 } # trailing of p\n# leading of q\n[q]\nx = 3\n"
      "y = 4\n");

  toml::deserializer de{out, std::cw<it_read_commented>};
  const auto [back, back_comments] = de.template load_commented<it_holder>();
  CHECK(back == it_holder_value());
  CHECK(comment(back_comments.q).leading == std::vector<std::string>{"leading of q"});
}

TEST_CASE("serde::inline_table: JSON and YAML ignore it")
{
  CHECK_EQ(it_json(it_holder_value()), it_json(it_plain_holder_value()));
  CHECK_EQ(it_yaml(it_holder_value()), it_yaml(it_plain_holder_value()));
}
