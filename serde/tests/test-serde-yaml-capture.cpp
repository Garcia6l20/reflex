#include <doctest/doctest.h>

import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::literals;

using lines_t = std::vector<std::string>;

struct ya_srv
{
  int         port = 0;
  std::string host;

  bool operator==(ya_srv const&) const = default;
};

struct ya_cfg
{
  int              value = 0;
  ya_srv           srv;
  std::vector<int> list;
  std::string      name;

  bool operator==(ya_cfg const&) const = default;
};

struct ya_leaf
{
  int x = 0;

  bool operator==(ya_leaf const&) const = default;
};

struct ya_mid
{
  ya_leaf b;

  bool operator==(ya_mid const&) const = default;
};

struct ya_deep
{
  ya_mid a;
  int    z = 0;

  bool operator==(ya_deep const&) const = default;
};

struct ya_flat
{
  int a = 0;
  int b = 0;

  bool operator==(ya_flat const&) const = default;
};

struct ya_items
{
  std::vector<int> items;
  int              next = 0;

  bool operator==(ya_items const&) const = default;
};

struct ya_text
{
  std::string text;
  int         next = 0;

  bool operator==(ya_text const&) const = default;
};

struct ya_opt
{
  std::optional<ya_leaf> on;
  std::optional<ya_leaf> off;
  int                    tail = 0;

  bool operator==(ya_opt const&) const = default;
};

struct ya_mapped
{
  std::map<std::string, int> m;
  int                        after = 0;

  bool operator==(ya_mapped const&) const = default;
};

constexpr yaml::read_options ya_keep{.comments = true};
constexpr yaml::read_options ya_keep_skip{
    .comments     = true,
    .unknown_keys = serde::unknown_keys::skip};

template <typename T> static serde::commented<T> ya_load(std::string_view text)
{
  yaml::deserializer de{text, std::cw<ya_keep>};
  return de.load_commented<T>();
}

static const auto ya_document = "# header\n"
                                "\n"
                                "# value comment\n"
                                "value: 1 # v trailing\n"
                                "\n"
                                "# Server settings\n"
                                "\n"
                                "# srv section\n"
                                "srv: # srv trailing\n"
                                "  # port lead\n"
                                "  port: 8080 # port trail\n"
                                "  host: h\n"
                                "  # srv footer\n"
                                "\n"
                                "# list lead\n"
                                "list: # list trail\n"
                                "  - 1 # inside\n"
                                "  # inner\n"
                                "  - 2\n"
                                "# name lead\n"
                                "name: n\n"
                                "# root footer\n"sv;

static ya_cfg ya_expected()
{
  return {
      1, {8080, "h"},
       {1,    2  },
       "n"
  };
}

TEST_CASE("reflex::serde::yaml capture: every anchoring row")
{
  const auto [v, c] = ya_load<ya_cfg>(ya_document);
  CHECK(v == ya_expected());

  CHECK_EQ(comment(c).leading, lines_t{"header", "\n"});
  CHECK_EQ(comment(c).trailing, "");
  CHECK_EQ(comment(c).footer, lines_t{"root footer"});

  CHECK_EQ(c.value.leading, lines_t{"value comment"});
  CHECK_EQ(c.value.trailing, "v trailing");

  CHECK_EQ(comment(c.srv).leading, lines_t{"\n", "Server settings", "\n", "srv section"});
  CHECK_EQ(comment(c.srv).trailing, "srv trailing");
  CHECK_EQ(comment(c.srv).footer, lines_t{"srv footer"});
  CHECK_EQ(c.srv.port.leading, lines_t{"port lead"});
  CHECK_EQ(c.srv.port.trailing, "port trail");
  CHECK_EQ(c.srv.host, comment_block{});

  CHECK_EQ(c.list.leading, lines_t{"\n", "list lead"});
  CHECK_EQ(c.list.trailing, "list trail");
  CHECK(c.list.footer.empty());

  CHECK_EQ(c.name.leading, lines_t{"name lead"});
  CHECK_EQ(c.name.trailing, "");

  CHECK_EQ(layout_of(c), serde::layout{.indent = 2, .final_newline = true});
}

TEST_CASE("reflex::serde::yaml capture: the stream cursor reads the same tree")
{
  std::istringstream in{std::string{ya_document}};
  yaml::deserializer de{in, std::cw<ya_keep>};
  const auto [v, c]   = de.load_commented<ya_cfg>();
  const auto expected = ya_load<ya_cfg>(ya_document);
  CHECK(v == ya_expected());
  CHECK(c == expected.comments);
}

TEST_CASE("reflex::serde::yaml capture: a section comment above a key keeps its blank line")
{
  const auto [v, c] = ya_load<ya_srv>("host: h\n"
                                      "# Server settings\n"
                                      "\n"
                                      "# port to listen on\n"
                                      "port: 1\n");
  CHECK_EQ(v.port, 1);
  CHECK_EQ(c.port.leading, lines_t{"Server settings", "\n", "port to listen on"});

  const auto [nv, nc] = ya_load<ya_cfg>("srv:\n"
                                        "  # Server settings\n"
                                        "\n"
                                        "  # port to listen on\n"
                                        "  port: 2\n");
  CHECK_EQ(nv.srv.port, 2);
  CHECK_EQ(nc.srv.port.leading, lines_t{"Server settings", "\n", "port to listen on"});
}

TEST_CASE("reflex::serde::yaml capture: a header with no blank line is the first key's")
{
  const auto [v, c] = ya_load<ya_flat>("# about a\na: 1\nb: 2");
  CHECK(comment(c).leading.empty());
  CHECK_EQ(c.a.leading, lines_t{"about a"});
  CHECK_EQ(layout_of(c), serde::layout{.indent = 0, .final_newline = false});
}

TEST_CASE("reflex::serde::yaml capture: block-end ownership by column")
{
  const auto [v, c] = ya_load<ya_deep>("a:\n"
                                       "  b:\n"
                                       "    x: 1\n"
                                       "    # b footer\n"
                                       "  # a footer\n"
                                       "# z lead\n"
                                       "z: 2\n");
  CHECK(v == ya_deep{{{1}}, 2});
  CHECK_EQ(comment(c.a.b).footer, lines_t{"b footer"});
  CHECK_EQ(comment(c.a).footer, lines_t{"a footer"});
  CHECK_EQ(c.z.leading, lines_t{"z lead"});

  const auto [dv, dc] = ya_load<ya_deep>("a:\n"
                                         "  b:\n"
                                         "    x: 1\n"
                                         "# column 0, two levels down\n"
                                         "z: 2\n");
  CHECK(comment(dc.a.b).footer.empty());
  CHECK(comment(dc.a).footer.empty());
  CHECK_EQ(dc.z.leading, lines_t{"column 0, two levels down"});

  const auto [ev, ec] = ya_load<ya_mid>("b:\n"
                                        "  x: 1\n"
                                        "# end of document\n");
  CHECK(comment(ec.b).footer.empty());
  CHECK_EQ(comment(ec).footer, lines_t{"end of document"});
}

TEST_CASE("reflex::serde::yaml capture: blank lines after a footer belong to the next key")
{
  const auto [v, c] = ya_load<ya_deep>("a:\n"
                                       "  b:\n"
                                       "    x: 1\n"
                                       "\n"
                                       "    # b footer\n"
                                       "\n"
                                       "z: 2\n");
  CHECK_EQ(comment(c.a.b).footer, lines_t{"\n", "b footer"});
  CHECK(comment(c.a).footer.empty());
  CHECK_EQ(c.z.leading, lines_t{"\n"});
}

TEST_CASE("reflex::serde::yaml capture: a comment between flush sequence entries")
{
  const auto [v, c] = ya_load<ya_items>("items:\n"
                                        "- 1\n"
                                        "# between\n"
                                        "- 2\n"
                                        "next: 3\n");
  CHECK(
      v
      == ya_items{
          {1, 2},
          3
  });
  CHECK_EQ(c.items, comment_block{});
  CHECK_EQ(c.next.leading, lines_t{"between"});

  const auto [tv, tc] = ya_load<ya_items>("items:\n"
                                          "- 1\n"
                                          "# between\n"
                                          "- 2 # two\n"
                                          "next: 3\n");
  CHECK(
      tv
      == ya_items{
          {1, 2},
          3
  });
  CHECK_EQ(tc.items, comment_block{});
  CHECK(tc.next.leading.empty());
}

TEST_CASE("reflex::serde::yaml capture: comments after a sequence go to the next key")
{
  const auto [v, c] = ya_load<ya_items>("items:\n"
                                        "  - 1 # one\n"
                                        "\n"
                                        "  - 2 # two\n"
                                        "\n"
                                        "# about next\n"
                                        "next: 3\n");
  CHECK(
      v
      == ya_items{
          {1, 2},
          3
  });
  CHECK_EQ(c.items, comment_block{});
  CHECK_EQ(c.next.leading, lines_t{"\n", "about next"});
}

TEST_CASE("reflex::serde::yaml capture: a block scalar header comment is the key's trailing")
{
  const auto [v, c] = ya_load<ya_text>("text: | # x\n"
                                       "  body # not a comment\n"
                                       "\n"
                                       "# about next\n"
                                       "next: 1\n");
  CHECK_EQ(v.text, "body # not a comment\n");
  CHECK_EQ(c.text.trailing, "x");
  CHECK_EQ(c.next.leading, lines_t{"\n", "about next"});
}

TEST_CASE("reflex::serde::yaml capture: optional aggregate members")
{
  const auto [v, c] = ya_load<ya_opt>("# on lead\n"
                                      "on: # on trailing\n"
                                      "  # x lead\n"
                                      "  x: 1\n"
                                      "off: null # off trailing\n"
                                      "tail: 2\n");
  CHECK(v == ya_opt{ya_leaf{1}, std::nullopt, 2});
  CHECK_EQ(comment(c.on).leading, lines_t{"on lead"});
  CHECK_EQ(comment(c.on).trailing, "on trailing");
  CHECK_EQ(c.on.x.leading, lines_t{"x lead"});
  CHECK_EQ(comment(c.off).trailing, "off trailing");
}

TEST_CASE("reflex::serde::yaml capture: comments inside a map value are dropped")
{
  const auto [v, c] = ya_load<ya_mapped>("# m lead\n"
                                         "m: # m trailing\n"
                                         "  # inside\n"
                                         "  k: 1 # inside too\n"
                                         "# after lead\n"
                                         "after: 2\n");
  CHECK_EQ(v.m.at("k"), 1);
  CHECK_EQ(c.m.leading, lines_t{"m lead"});
  CHECK_EQ(c.m.trailing, "m trailing");
  CHECK(c.m.footer.empty());
  CHECK_EQ(c.after.leading, lines_t{"after lead"});
}

TEST_CASE("reflex::serde::yaml capture: comments above a skipped key are dropped with it")
{
  yaml::deserializer de{
      "# about a\n"
      "a: 1\n"
      "# about extra\n"
      "extra: # extra trailing\n"
      "  deep: 1 # deep trailing\n"
      "  # inside extra\n"
      "# about b\n"
      "b: 2 # b trailing\n"
      "# about gone\n"
      "gone: 3 # gone trailing\n"sv,
      std::cw<ya_keep_skip>};
  const auto [v, c] = de.load_commented<ya_flat>();
  CHECK(v == ya_flat{1, 2});
  CHECK_EQ(c.a.leading, lines_t{"about a"});
  CHECK_EQ(c.a.trailing, "");
  CHECK_EQ(c.b.leading, lines_t{"about b"});
  CHECK_EQ(c.b.trailing, "b trailing");
  CHECK(comment(c).footer.empty());
}

TEST_CASE("reflex::serde::yaml capture: layout at 2 and 4 spaces")
{
  const auto two  = ya_load<ya_deep>("a:\n  b:\n    x: 1\nz: 2\n");
  const auto four = ya_load<ya_deep>("a:\n    b:\n        x: 1\nz: 2");
  CHECK_EQ(layout_of(two.comments), serde::layout{.indent = 2, .final_newline = true});
  CHECK_EQ(layout_of(four.comments), serde::layout{.indent = 4, .final_newline = false});
  CHECK(four.value == ya_deep{{{1}}, 2});
}

TEST_CASE("reflex::serde::yaml capture: a plain load under comments reads the same value")
{
  yaml::deserializer keep{ya_document, std::cw<ya_keep>};
  yaml::deserializer plain{ya_document};
  const auto         kept = keep.load<ya_cfg>();
  CHECK(kept == plain.load<ya_cfg>());
  CHECK(kept == ya_expected());
}

TEST_CASE("reflex::serde::yaml capture: comments before a document marker are dropped")
{
  const auto [v, c] = ya_load<ya_flat>("# before\n---\n# after\na: 1\nb: 2\n");
  CHECK(comment(c).leading.empty());
  CHECK_EQ(c.a.leading, lines_t{"after"});
}

struct ya_fleet
{
  std::vector<ya_srv> servers;
  int                 count = 0;

  bool operator==(ya_fleet const&) const = default;
};

TEST_CASE("reflex::serde::yaml capture: comments inside a sequence of mappings are dropped")
{
  const auto [v, c] = ya_load<ya_fleet>("servers: # all of them\n"
                                        "  - port: 1 # first port\n"
                                        "    # host lead\n"
                                        "    host: a\n"
                                        "  - port: 2\n"
                                        "    host: b\n"
                                        "    # element footer\n"
                                        "# count lead\n"
                                        "count: 2\n");
  CHECK(
      v
      == ya_fleet{
          {{1, "a"}, {2, "b"}},
          2
  });
  CHECK_EQ(c.servers.trailing, "all of them");
  CHECK(c.servers.leading.empty());
  CHECK(c.servers.footer.empty());
  CHECK_EQ(c.count.leading, lines_t{"count lead"});
}

TEST_CASE("reflex::serde::yaml capture: a sequence root keeps no inner comment in a node")
{
  yaml::deserializer de{"# head\n- port: 1\n  host: a\n"sv, std::cw<ya_keep>};
  const auto [v, c] = de.load_commented<std::vector<ya_srv>>();
  CHECK(
      v
      == std::vector<ya_srv>{
          {1, "a"}
  });
  CHECK_EQ(c.footer, lines_t{"head"});
}

struct ya_entry
{
  int a = 0;

  bool operator==(ya_entry const&) const = default;
};

struct ya_entries
{
  std::vector<ya_entry> items;
  int                   after = 0;

  bool operator==(ya_entries const&) const = default;
};

struct ya_holder
{
  ya_entry e;
  int      b = 0;

  bool operator==(ya_holder const&) const = default;
};

template <typename T> static serde::commented<T> ya_load_skip(std::string_view text)
{
  yaml::deserializer de{text, std::cw<ya_keep_skip>};
  return de.load_commented<T>();
}

TEST_CASE("reflex::serde::yaml capture: an empty skipped key keeps its parent's sequence entries")
{
  CHECK(
      ya_load_skip<std::vector<ya_entry>>("- a: 1\n  junk:\n- a: 2\n- a: 3").value
      == std::vector<ya_entry>{{1}, {2}, {3}});
  CHECK(
      ya_load_skip<std::vector<ya_entry>>("- a: 1\n  junk:\n  - x\n- a: 2\n").value
      == std::vector<ya_entry>{{1}, {2}});
  CHECK(
      ya_load_skip<ya_entries>("items:\n  - a: 1\n    junk:\n  - a: 2\nafter: 5\n").value
      == ya_entries{
          {{1}, {2}},
          5
  });
  CHECK(
      ya_load_skip<ya_entries>("items:\n- a: 1\n  junk:\n- a: 2\nafter: 5\n").value
      == ya_entries{
          {{1}, {2}},
          5
  });
  CHECK(ya_load_skip<ya_holder>("e:\n  a: 1\n  junk:\n  - x\nb: 2\n").value == ya_holder{{1}, 2});
  CHECK(ya_load_skip<ya_holder>("e:\n  a: 1\n  junk:\n  - x\n").value == ya_holder{{1}, 0});
  CHECK_THROWS_WITH(
      std::ignore = ya_load_skip<ya_holder>("e:\n  a: 1\n  junk:\n- x\nb: 2\n"),
      "YAML: expected ':' after the key '- x'");
}
