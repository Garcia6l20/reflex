#include <doctest/doctest.h>

import reflex.serde.json;

import std;

using namespace reflex;
using namespace reflex::serde;

struct jp_inner
{
  int         port;
  std::string host;

  constexpr bool operator==(jp_inner const&) const = default;
};

struct jp_doc
{
  int                                             value;
  jp_inner                                        srv;
  std::vector<int>                                list;
  std::vector<jp_inner>                           items;
  std::map<std::string, int>                      ports;
  std::vector<int>                                none;
  std::map<std::string, int>                      empty;
  std::optional<jp_inner>                         backup;
  std::optional<int>                              missing;
  [[= serde::omit_if_empty{}]] std::optional<int> skipped;

  constexpr bool operator==(jp_doc const&) const = default;
};

struct jp_all_omitted
{
  [[= serde::omit_if_empty{}]] std::optional<int> a;
  [[= serde::omit_if_empty{}]] std::vector<int>   b;

  constexpr bool operator==(jp_all_omitted const&) const = default;
};

struct jp_text
{
  std::string text;

  constexpr bool operator==(jp_text const&) const = default;
};

struct jp_tagged
{
  int a;
};

using jp_var  = poly::var<json::string, json::number, json::boolean, jp_tagged>;
using str_out = std::back_insert_iterator<std::string>;

static_assert(sizeof(json::serializer<str_out>) == sizeof(serde::detail::serializer_base<str_out>));

template <int Indent, typename T> static std::string jp_dump(T const& value)
{
  std::string out;
  json::serializer{out, std::cw<json::options{.indent = Indent}>}.dump(value);
  return out;
}

template <typename T> static T jp_load(std::string const& text)
{
  return json::deserializer{text}.template load<T>();
}

static const jp_doc jp_sample{
    .value   = 1,
    .srv     = {8080, "a"},
    .list    = {1, 2},
    .items   = {{1, "x"}, {2, "y"}},
    .ports   = {{"http", 80}, {"https", 443}},
    .none    = {},
    .empty   = {},
    .backup  = jp_inner{22, "b"},
    .missing = std::nullopt,
    .skipped = std::nullopt,
};

TEST_CASE("reflex::serde::json pretty: indent 2")
{
  const std::string expected = R"({
  "value": 1,
  "srv": {
    "port": 8080,
    "host": "a"
  },
  "list": [
    1,
    2
  ],
  "items": [
    {
      "port": 1,
      "host": "x"
    },
    {
      "port": 2,
      "host": "y"
    }
  ],
  "ports": {
    "http": 80,
    "https": 443
  },
  "none": [],
  "empty": {},
  "backup": {
    "port": 22,
    "host": "b"
  },
  "missing": null
})";
  const std::string out      = jp_dump<2>(jp_sample);
  CHECK_EQ(out, expected);
  CHECK(jp_load<jp_doc>(out) == jp_sample);
}

TEST_CASE("reflex::serde::json pretty: indent 4")
{
  const std::string expected = R"({
    "port": 8080,
    "host": "a"
})";
  CHECK_EQ(jp_dump<4>(jp_inner{8080, "a"}), expected);
  CHECK_EQ(jp_dump<4>(std::vector<int>{1, 2}), "[\n    1,\n    2\n]");

  const std::string nested = jp_dump<4>(jp_sample);
  CHECK(nested.find("\n    \"srv\": {\n        \"port\": 8080,\n") != std::string::npos);
  CHECK(jp_load<jp_doc>(nested) == jp_sample);
}

TEST_CASE("reflex::serde::json pretty: empty containers and omitted members")
{
  CHECK_EQ(jp_dump<2>(jp_all_omitted{}), "{}");
  CHECK_EQ(jp_dump<2>(std::vector<int>{}), "[]");
  CHECK_EQ(jp_dump<2>(std::map<std::string, int>{}), "{}");
  CHECK_EQ(jp_dump<2>(jp_all_omitted{.a = 3, .b = {}}), "{\n  \"a\": 3\n}");
  CHECK(jp_load<jp_all_omitted>(jp_dump<2>(jp_all_omitted{})) == jp_all_omitted{});
}

TEST_CASE("reflex::serde::json pretty: optionals at the root")
{
  CHECK_EQ(jp_dump<2>(std::optional<jp_inner>{}), "null");
  CHECK_EQ(
      jp_dump<2>(std::optional<jp_inner>{
          jp_inner{1, "h"}
  }),
      "{\n  \"port\": 1,\n  \"host\": \"h\"\n}");
}

TEST_CASE("reflex::serde::json pretty: a line break in a string stays escaped")
{
  const jp_text value{"a\nb"};
  const auto    out = jp_dump<2>(value);
  CHECK_EQ(out, "{\n  \"text\": \"a\\nb\"\n}");
  CHECK(jp_load<jp_text>(out) == value);
}

TEST_CASE("reflex::serde::json pretty: a json::value document")
{
  const auto doc = jp_load<json::value>(R"({"a":[1,{"b":true}],"c":"d"})");
  const auto out = jp_dump<2>(doc);
  CHECK_EQ(out, R"({
  "a": [
    1,
    {
      "b": true
    }
  ],
  "c": "d"
})");
  CHECK_EQ(jp_dump<0>(jp_load<json::value>(out)), R"({"a":[1,{"b":true}],"c":"d"})");
}

TEST_CASE("reflex::serde::json pretty: the __type tag of a poly::var aggregate")
{
  const jp_var doc{
      {"t", jp_tagged{5}}
  };
  const std::string id = std::to_string(std::hash<std::string_view>{}("jp_tagged"));
  CHECK_EQ(jp_dump<2>(doc), "{\n  \"t\": {\n    \"__type\": " + id + ",\n    \"a\": 5\n  }\n}");
  const auto back = jp_load<jp_var>(jp_dump<2>(doc));
  CHECK_EQ(back.at("t")->as<jp_tagged>().a, 5);
}

TEST_CASE("reflex::serde::json pretty: a pair is a one-entry object")
{
  const std::vector<std::pair<std::string, int>> pairs{
      {"a", 1}
  };
  CHECK_EQ(jp_dump<2>(pairs), "[\n  {\n    \"a\": 1\n  }\n]");
  CHECK_EQ(jp_dump<0>(pairs), R"([{"a":1}])");
}
