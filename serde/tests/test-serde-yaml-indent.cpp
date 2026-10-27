#include <doctest/doctest.h>

import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

struct yi_inner
{
  int         a = 0;
  std::string b;

  bool operator==(yi_inner const&) const = default;
};

struct yi_group
{
  std::string      name;
  std::vector<int> ids;

  bool operator==(yi_group const&) const = default;
};

struct yi_doc
{
  yi_inner                      nested;
  std::vector<int>              nums;
  std::vector<yi_inner>         items;
  std::vector<std::vector<int>> grid;
  std::vector<yi_group>         groups;
  std::vector<int>              none;
  std::map<std::string, int>    empty_map;

  bool operator==(yi_doc const&) const = default;
};

struct yi_server
{
  std::string host;
  int         port = 0;

  bool operator==(yi_server const&) const = default;
};

struct yi_servers
{
  std::vector<yi_server> servers;

  bool operator==(yi_servers const&) const = default;
};

static const yi_doc yi_sample{
    .nested    = {1, "x"},
    .nums      = {1, 2},
    .items     = {{1, "x"}, {2, "z"}},
    .grid      = {{1, 2}, {3}},
    .groups    = {{"g", {1, 2}}},
    .none      = {},
    .empty_map = {},
};

template <int Width> static std::string yi_dump(auto const& value)
{
  std::string out;
  yaml::serializer{out, std::cw<yaml::options{.indent = Width}>}.dump(value);
  return out;
}

template <typename T> static T yi_load(std::string_view text)
{
  return yaml::deserializer{text}.load<T>();
}

TEST_CASE("reflex::serde::yaml indent: width 2 is today's output")
{
  const auto expected = "nested:\n"
                        "  a: 1\n"
                        "  b: x\n"
                        "nums:\n"
                        "  - 1\n"
                        "  - 2\n"
                        "items:\n"
                        "  - a: 1\n"
                        "    b: x\n"
                        "  - a: 2\n"
                        "    b: z\n"
                        "grid:\n"
                        "  - - 1\n"
                        "    - 2\n"
                        "  - - 3\n"
                        "groups:\n"
                        "  - name: g\n"
                        "    ids:\n"
                        "      - 1\n"
                        "      - 2\n"
                        "none: []\n"
                        "empty_map: {}"s;
  CHECK_EQ(yi_dump<2>(yi_sample), expected);
  std::string plain;
  yaml::serializer{plain}.dump(yi_sample);
  CHECK_EQ(plain, expected);
  CHECK(yi_load<yi_doc>(expected) == yi_sample);
}

TEST_CASE("reflex::serde::yaml indent: width 4 pads the dash")
{
  const auto expected = "nested:\n"
                        "    a: 1\n"
                        "    b: x\n"
                        "nums:\n"
                        "    -   1\n"
                        "    -   2\n"
                        "items:\n"
                        "    -   a: 1\n"
                        "        b: x\n"
                        "    -   a: 2\n"
                        "        b: z\n"
                        "grid:\n"
                        "    -   -   1\n"
                        "        -   2\n"
                        "    -   -   3\n"
                        "groups:\n"
                        "    -   name: g\n"
                        "        ids:\n"
                        "            -   1\n"
                        "            -   2\n"
                        "none: []\n"
                        "empty_map: {}"s;
  CHECK_EQ(yi_dump<4>(yi_sample), expected);
  CHECK(yi_load<yi_doc>(expected) == yi_sample);
}

TEST_CASE("reflex::serde::yaml indent: width 3 pads the dash")
{
  const auto expected = "nested:\n"
                        "   a: 1\n"
                        "   b: x\n"
                        "nums:\n"
                        "   -  1\n"
                        "   -  2\n"
                        "items:\n"
                        "   -  a: 1\n"
                        "      b: x\n"
                        "   -  a: 2\n"
                        "      b: z\n"
                        "grid:\n"
                        "   -  -  1\n"
                        "      -  2\n"
                        "   -  -  3\n"
                        "groups:\n"
                        "   -  name: g\n"
                        "      ids:\n"
                        "         -  1\n"
                        "         -  2\n"
                        "none: []\n"
                        "empty_map: {}"s;
  CHECK_EQ(yi_dump<3>(yi_sample), expected);
  CHECK(yi_load<yi_doc>(expected) == yi_sample);
}

TEST_CASE("reflex::serde::yaml indent: empty collections stay inline at every width")
{
  const yi_doc empty{};
  const auto   tail = "nums: []\n"
                      "items: []\n"
                      "grid: []\n"
                      "groups: []\n"
                      "none: []\n"
                      "empty_map: {}"s;
  CHECK_EQ(yi_dump<2>(empty), "nested:\n  a: 0\n  b: ''\n" + tail);
  CHECK_EQ(yi_dump<3>(empty), "nested:\n   a: 0\n   b: ''\n" + tail);
  CHECK_EQ(yi_dump<4>(empty), "nested:\n    a: 0\n    b: ''\n" + tail);
  CHECK(yi_load<yi_doc>(yi_dump<3>(empty)) == empty);
  CHECK(yi_load<yi_doc>(yi_dump<4>(empty)) == empty);
}

TEST_CASE("reflex::serde::yaml indent: a hand-written padded document reads")
{
  const yi_servers expected{
      {{"a", 1}, {"b", 2}}
  };
  CHECK(
      yi_load<yi_servers>("servers:\n"
                          "    -   host: a\n"
                          "        port: 1\n"
                          "    -   host: b\n"
                          "        port: 2\n")
      == expected);
  CHECK(
      yi_load<yi_servers>("servers:\n"
                          "-   host: a\n"
                          "    port: 1\n"
                          "-   host: b\n"
                          "    port: 2\n")
      == expected);
  CHECK(
      yi_load<yi_servers>("servers:\n"
                          "   -  host: a\n"
                          "      port: 1\n"
                          "   -  host: b\n"
                          "      port: 2\n")
      == expected);
}
