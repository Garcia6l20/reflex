#include <doctest/doctest.h>

import reflex.serde.json;
import reflex.serde.toml;
import reflex.serde.xml;
import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;

struct uk_inner
{
  int         port = 0;
  std::string host;

  constexpr bool operator==(uk_inner const&) const = default;
};

struct uk_config
{
  int         first = 0;
  uk_inner    srv;
  std::string last;

  constexpr bool operator==(uk_config const&) const = default;
};

struct uk_pooled
{
  int                   first = 0;
  std::vector<uk_inner> pool;

  constexpr bool operator==(uk_pooled const&) const = default;
};

struct uk_entry
{
  int a = 0;

  constexpr bool operator==(uk_entry const&) const = default;
};

struct uk_entries
{
  std::vector<uk_entry> items;
  int                   after = 0;

  constexpr bool operator==(uk_entries const&) const = default;
};

const uk_config uk_expected{
    1,
    {8080, "h"},
    "z"
};

constexpr json::read_options json_skip{.unknown_keys = unknown_keys::skip};

template <typename T> T json_skip_load(std::string const& text)
{
  json::deserializer de{text, std::cw<json_skip>};
  return de.template load<T>();
}

constexpr yaml::read_options yaml_skip{.unknown_keys = unknown_keys::skip};

template <typename T> T yaml_skip_load(std::string const& text)
{
  yaml::deserializer de{text, std::cw<yaml_skip>};
  return de.template load<T>();
}

template <typename T> std::string yaml_default_message(std::string const& text)
{
  try
  {
    (void)yaml::deserializer{text}.template load<T>();
  }
  catch(std::exception const& e)
  {
    return e.what();
  }
  return {};
}

constexpr toml::read_options toml_skip{.unknown_keys = unknown_keys::skip};

template <typename T> T toml_skip_load(std::string const& text)
{
  toml::deserializer de{text, std::cw<toml_skip>};
  return de.template load<T>();
}

template <typename T> std::string toml_default_message(std::string const& text)
{
  try
  {
    (void)toml::deserializer{text}.template load<T>();
  }
  catch(std::exception const& e)
  {
    return e.what();
  }
  return {};
}

template <typename T> std::string json_default_message(std::string const& text)
{
  try
  {
    (void)json::deserializer{text}.template load<T>();
  }
  catch(std::exception const& e)
  {
    return e.what();
  }
  return {};
}

constexpr xml::read_options xml_error{.unknown_keys = unknown_keys::error};

template <typename T> std::string xml_error_message(std::string const& text)
{
  try
  {
    xml::deserializer de{text, std::cw<xml_error>};
    (void)de.template load<T>();
  }
  catch(std::exception const& e)
  {
    return e.what();
  }
  return {};
}

TEST_CASE("reflex::serde::unknown_keys: defaults keep each format's behaviour")
{
  static_assert(json::read_options{}.unknown_keys == unknown_keys::error);
  static_assert(xml::read_options{}.unknown_keys == unknown_keys::skip);
  static_assert(yaml::read_options{}.unknown_keys == unknown_keys::error);
  static_assert(toml::read_options{}.unknown_keys == unknown_keys::error);
}

TEST_CASE("reflex::serde::object_try_visit_flat: reports a miss instead of throwing")
{
  uk_inner value{};
  int      visits = 0;
  CHECK(object_try_visit_flat("port", value, [&](auto& member) {
    ++visits;
    if constexpr(std::same_as<std::remove_cvref_t<decltype(member)>, int>)
    {
      member = 7;
    }
  }));
  CHECK_EQ(value.port, 7);
  CHECK_FALSE(object_try_visit_flat("nope", value, [&](auto&) { ++visits; }));
  CHECK_EQ(visits, 1);

  std::map<std::string, int> map;
  CHECK(object_try_visit_flat("any", map, [](int& v) { v = 3; }));
  CHECK_EQ(map.at("any"), 3);
}

TEST_CASE("reflex::serde::json: unknown_keys::skip")
{
  SUBCASE("an unknown scalar between two known keys")
  {
    CHECK_EQ(
        json_skip_load<uk_config>(
            R"({"first":1,"extra":"x","srv":{"port":8080,"host":"h"},"last":"z"})"),
        uk_expected);
  }
  SUBCASE("an unknown nested object")
  {
    CHECK_EQ(
        json_skip_load<uk_config>(
            R"({"first":1,"extra":{"a":[1,{"b":null}],"c":{}},"srv":{"port":8080,"host":"h"},"last":"z"})"),
        uk_expected);
  }
  SUBCASE("an unknown key first and last")
  {
    CHECK_EQ(
        json_skip_load<uk_config>(
            R"({"head":[true,false],"first":1,"srv":{"port":8080,"host":"h"},"last":"z","tail":2.5})"),
        uk_expected);
  }
  SUBCASE("an unknown key inside a nested known object")
  {
    CHECK_EQ(
        json_skip_load<uk_config>(
            R"({"first":1,"srv":{"port":8080,"tls":{"on":true},"host":"h"},"last":"z"})"),
        uk_expected);
  }
  SUBCASE("the default still throws the same message")
  {
    CHECK_EQ(
        json_default_message<uk_config>(
            R"({"first":1,"extra":"x","srv":{"port":8080,"host":"h"},"last":"z"})"),
        "Key not found in object");
  }
}

TEST_CASE("reflex::serde::xml: unknown_keys::error")
{
  const std::string doc =
      "<uk_config><first>1</first><extra>x</extra><srv><port>8080</port><host>h</host></srv>"
      "<last>z</last></uk_config>";

  SUBCASE("the default skips the unknown element")
  {
    CHECK_EQ(xml::deserializer{doc}.template load<uk_config>(), uk_expected);
  }
  SUBCASE("error throws and names XML and the element")
  {
    const std::string message = xml_error_message<uk_config>(doc);
    CHECK(message.starts_with("XML: "));
    CHECK(message.find("'extra'") != std::string::npos);
  }
  SUBCASE("error throws on an unknown element in a nested aggregate")
  {
    const std::string message = xml_error_message<uk_config>(
        "<uk_config><first>1</first><srv><port>8080</port><tls/><host>h</host></srv>"
        "<last>z</last></uk_config>");
    CHECK(message.starts_with("XML: "));
    CHECK(message.find("'tls'") != std::string::npos);
  }
  SUBCASE("error accepts a document with no unknown element")
  {
    const std::string clean =
        "<uk_config><first>1</first><srv><port>8080</port><host>h</host></srv>"
        "<last>z</last></uk_config>";
    xml::deserializer de{clean, std::cw<xml_error>};
    CHECK_EQ(de.template load<uk_config>(), uk_expected);
  }
}

TEST_CASE("reflex::serde::yaml: unknown_keys::skip")
{
  SUBCASE("an unknown scalar between two known keys")
  {
    CHECK_EQ(
        yaml_skip_load<uk_config>("first: 1\nextra: x\nsrv:\n  port: 8080\n  host: h\nlast: z\n"),
        uk_expected);
  }
  SUBCASE("an unknown nested mapping")
  {
    CHECK_EQ(
        yaml_skip_load<uk_config>("first: 1\nextra:\n  a: 1\n  b:\n    c: [1, 2]\n    d: e\n"
                                  "srv:\n  port: 8080\n  host: h\nlast: z\n"),
        uk_expected);
  }
  SUBCASE("an unknown key first and last")
  {
    CHECK_EQ(
        yaml_skip_load<uk_config>(
            "head: 0\nfirst: 1\nsrv:\n  port: 8080\n  host: h\nlast: z\ntail:\n  x: 1\n"),
        uk_expected);
  }
  SUBCASE("an unknown key inside a nested known mapping")
  {
    CHECK_EQ(
        yaml_skip_load<uk_config>(
            "first: 1\nsrv:\n  port: 8080\n  tls:\n    on: true\n  host: h\nlast: z\n"),
        uk_expected);
  }
  SUBCASE("an unknown block scalar")
  {
    CHECK_EQ(
        yaml_skip_load<uk_config>("first: 1\nextra: |\n  line one\n  line: two\n\n  three\n"
                                  "srv:\n  port: 8080\n  host: h\nlast: z\n"),
        uk_expected);
    CHECK_EQ(
        yaml_skip_load<uk_config>("first: 1\nsrv:\n  port: 8080\n  note: >-\n    folded\n"
                                  "    text\n  host: h\nlast: z\n"),
        uk_expected);
  }
  SUBCASE("an unknown flow collection")
  {
    CHECK_EQ(
        yaml_skip_load<uk_config>("first: 1\nextra: {a: [{b: 2}, 1], c: 'x'}\nsrv:\n  port: 8080\n"
                                  "  host: h\nlast: z\n"),
        uk_expected);
    CHECK_EQ(
        yaml_skip_load<uk_config>("first: 1\nextra: [{d: 4}, 1, [2, 3]]\nsrv: {port: 8080, tls: "
                                  "[1], host: h}\nlast: z\n"),
        uk_expected);
  }
  SUBCASE("an unknown flow collection holding a mapping after a scalar")
  {
    CHECK_EQ(
        yaml_skip_load<uk_config>("first: 1\nextra: {a: [1, {b: 2}], c: 'x'}\nsrv:\n  port: 8080\n"
                                  "  host: h\nlast: z\n"),
        uk_expected);
    const std::string  flow = "[1, {b: 2}, c]";
    yaml::deserializer de{flow};
    const auto         v = de.template load<yaml::value>();
    REQUIRE(v.is_array());
    CHECK_EQ(v.as_array().size(), 3);
    CHECK(v.as_array()[1].is_object());
  }
  SUBCASE("an unknown block sequence, indented and flush")
  {
    CHECK_EQ(
        yaml_skip_load<uk_config>("first: 1\nextra:\n  - 1\n  - a: 2\n    b: 3\n  - - 4\n"
                                  "srv:\n  port: 8080\n  host: h\nlast: z\n"),
        uk_expected);
    CHECK_EQ(
        yaml_skip_load<uk_config>(
            "first: 1\nextra:\n- 1\n- 2\nsrv:\n  port: 8080\n  host: h\nlast: z\n"),
        uk_expected);
    CHECK_EQ(
        yaml_skip_load<uk_config>(
            "first: 1\nsrv:\n  port: 8080\n  tags:\n  - a: 1\n    b: 2\n  - c\n"
            "  host: h\nextra:\n- 1\n"),
        uk_config{
            1, {8080, "h"},
             ""
    });
  }
  SUBCASE("an empty unknown key keeps its parent's sequence entries")
  {
    CHECK_EQ(
        yaml_skip_load<std::vector<uk_entry>>("- a: 1\n  junk:\n- a: 2\n- a: 3"),
        std::vector<uk_entry>{{1}, {2}, {3}});
    CHECK_EQ(
        yaml_skip_load<std::vector<uk_entry>>("- a: 1\n  junk:\n  - x\n  - y\n- a: 2\n"),
        std::vector<uk_entry>{{1}, {2}});
    CHECK_EQ(
        yaml_skip_load<uk_entries>("items:\n  - a: 1\n    junk:\n  - a: 2\nafter: 5\n"),
        uk_entries{
            {{1}, {2}},
            5
    });
    CHECK_EQ(
        yaml_skip_load<uk_entries>("items:\n- a: 1\n  junk:\n- a: 2\nafter: 5\n"),
        uk_entries{
            {{1}, {2}},
            5
    });
    CHECK_EQ(
        yaml_skip_load<uk_entries>("items:\n- a: 1\n  junk:\n  - x\n- a: 2\nafter: 5\n"),
        uk_entries{
            {{1}, {2}},
            5
    });
  }
  SUBCASE("comments around a skipped key")
  {
    CHECK_EQ(
        yaml_skip_load<uk_config>("first: 1 # one\n# above\nextra: x # trailing\n# below\n"
                                  "srv:\n  port: 8080\n  host: h\nlast: z\n"),
        uk_expected);
  }
  SUBCASE("the default still throws the same message")
  {
    CHECK_EQ(
        yaml_default_message<uk_config>(
            "first: 1\nextra: x\nsrv:\n  port: 8080\n  host: h\nlast: z\n"),
        "Key not found in object");
    CHECK_EQ(
        yaml_default_message<uk_config>("first: 1\nsrv: {port: 8080, tls: 1, host: h}\nlast: z\n"),
        "Key not found in object");
  }
}

TEST_CASE("reflex::serde::toml: unknown_keys::skip")
{
  const std::string srv = "[srv]\nport = 8080\nhost = \"h\"\n";

  SUBCASE("an unknown scalar between two known keys")
  {
    CHECK_EQ(
        toml_skip_load<uk_config>("first = 1\nextra = \"x\"\nlast = \"z\"\n" + srv), uk_expected);
  }
  SUBCASE("an unknown inline table and a multi-line array")
  {
    CHECK_EQ(
        toml_skip_load<uk_config>(
            "first = 1\nextra = {a = [1, {b = 2}], c = \"x\"}\n"
            "more = [\n  1, # one\n  [2, 3],\n]\nlast = \"z\"\n"
            + srv),
        uk_expected);
  }
  SUBCASE("an unknown key first and last")
  {
    CHECK_EQ(
        toml_skip_load<uk_config>("head = 0\nfirst = 1\nlast = \"z\"\n" + srv + "tail = [1, 2]\n"),
        uk_expected);
  }
  SUBCASE("an unknown dotted key, at the root and inside a known table")
  {
    CHECK_EQ(
        toml_skip_load<uk_config>("first = 1\nextra.a.b = 1\nextra.a.c = 2\nlast = \"z\"\n"
                                  "[srv]\nport = 8080\ntls.on = true\nhost = \"h\"\n"),
        uk_expected);
  }
  SUBCASE("an unknown key inside a known inline table")
  {
    CHECK_EQ(
        toml_skip_load<uk_config>(
            "first = 1\nsrv = {port = 8080, tls = {on = true}, host = \"h\"}\nlast = \"z\"\n"),
        uk_expected);
  }
  SUBCASE("an unknown table with a sub-table, followed by a known table")
  {
    CHECK_EQ(
        toml_skip_load<uk_config>(
            "first = 1\nlast = \"z\"\n[gone]\na = 1\n[gone.child]\n"
            "b = [1, 2]\n[gone.child.deeper]\nc = {d = 1}\n"
            + srv),
        uk_expected);
  }
  SUBCASE("an unknown sub-table of a known table")
  {
    CHECK_EQ(
        toml_skip_load<uk_config>("first = 1\nlast = \"z\"\n" + srv + "[srv.tls]\non = true\n"),
        uk_expected);
  }
  SUBCASE("an unknown array of tables with a sub-table, followed by a known table")
  {
    CHECK_EQ(
        toml_skip_load<uk_config>(
            "first = 1\nlast = \"z\"\n[[items]]\nname = \"a\"\n"
            "[[items]]\nname = \"b\"\n[items.sub]\nx = 1\n"
            + srv),
        uk_expected);
  }
  SUBCASE("an unknown key inside a known array of tables")
  {
    CHECK_EQ(
        toml_skip_load<uk_pooled>("first = 1\n[[pool]]\nport = 1\nextra = 0\nhost = \"a\"\n"
                                  "[pool.gone]\nx = 1\n[[pool]]\nport = 2\nhost = \"b\"\n"),
        uk_pooled{
            1, {{1, "a"}, {2, "b"}}
    });
  }
  SUBCASE("a shape conflict is still an error")
  {
    toml::deserializer de{std::string_view{"first.x = 1\n"}, std::cw<toml_skip>};
    CHECK_THROWS_WITH_AS(
        (void)de.template load<uk_config>(), doctest::Contains("is not a table"),
        std::runtime_error);
  }
  SUBCASE("the default still throws the same message")
  {
    CHECK_EQ(
        toml_default_message<uk_config>("first = 1\nextra = \"x\"\nlast = \"z\"\n" + srv),
        "TOML: key 'extra': Key not found in object");
    CHECK_EQ(
        toml_default_message<uk_config>("first = 1\nlast = \"z\"\n[gone]\na = 1\n" + srv),
        "TOML: key 'gone': Key not found in object");
    CHECK_EQ(
        toml_default_message<uk_config>("first = 1\nlast = \"z\"\n" + srv + "tls.on = true\n"),
        "TOML: key 'srv.tls.on': Key not found in object");
  }
}
