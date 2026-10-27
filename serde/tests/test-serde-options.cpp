#include <doctest/doctest.h>

import reflex.serde.json;
import reflex.serde.toml;
import reflex.serde.xml;
import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;

struct opt_point
{
  int         x;
  std::string name;

  constexpr bool operator==(opt_point const&) const = default;
};

namespace
{
using str_out    = std::back_insert_iterator<std::string>;
using stream_out = std::ostreambuf_iterator<char>;
using str_in     = std::string::const_iterator;
using view_in    = std::string_view::const_iterator;

constexpr json::options      json_pretty{.indent = 2};
constexpr json::read_options json_lenient{.comments = true, .trailing_commas = true};
constexpr yaml::options      yaml_wide{.indent = 4};
constexpr yaml::read_options yaml_keep{.comments = true};
constexpr toml::options      toml_keep{.comments = true};
constexpr toml::read_options toml_read_keep{.comments = true};
constexpr xml::options       xml_pretty{.indent = 2};
constexpr xml::read_options  xml_keep{.comments = true};

const opt_point sample{7, "a\"b"};
} // namespace

TEST_CASE("reflex::serde options: an untagged serializer deduces default options")
{
  std::string        s;
  std::ostringstream os;
  static_assert(
      std::same_as<decltype(json::serializer{s}), json::serializer<str_out, json::options{}>>);
  static_assert(std::same_as<decltype(json::serializer{os}), json::serializer<stream_out>>);
  static_assert(
      std::same_as<decltype(yaml::serializer{s}), yaml::serializer<str_out, yaml::options{}>>);
  static_assert(
      std::same_as<decltype(toml::serializer{s}), toml::serializer<str_out, toml::options{}>>);
  static_assert(
      std::same_as<decltype(xml::serializer{s}), xml::serializer<str_out, xml::options{}>>);
  static_assert(json::serializer<str_out>::opts == json::options{});
  static_assert(yaml::serializer<str_out>::opts.indent == 2);
  static_assert(json::deserializer<str_in>::read_opts == json::read_options{});
}

TEST_CASE("reflex::serde options: a tagged serializer deduces the tag's options")
{
  std::string        s;
  std::ostringstream os;
  static_assert(decltype(json::serializer{s, std::cw<json_pretty>})::opts.indent == 2);
  static_assert(std::same_as<
                decltype(json::serializer{os, std::cw<json_pretty>}),
                json::serializer<stream_out, json_pretty>>);
  static_assert(
      std::same_as<
          decltype(yaml::serializer{s, std::cw<yaml_wide>}), yaml::serializer<str_out, yaml_wide>>);
  static_assert(std::same_as<
                decltype(yaml::serializer{os, std::cw<yaml_wide>}),
                yaml::serializer<stream_out, yaml_wide>>);
  static_assert(
      std::same_as<
          decltype(toml::serializer{s, std::cw<toml_keep>}), toml::serializer<str_out, toml_keep>>);
  static_assert(std::same_as<
                decltype(toml::serializer{os, std::cw<toml_keep>}),
                toml::serializer<stream_out, toml_keep>>);
  static_assert(
      std::same_as<
          decltype(xml::serializer{s, std::cw<xml_pretty>}), xml::serializer<str_out, xml_pretty>>);
  static_assert(std::same_as<
                decltype(xml::serializer{os, std::cw<xml_pretty>}),
                xml::serializer<stream_out, xml_pretty>>);
}

TEST_CASE("reflex::serde options: a tagged deserializer deduces the tag's options")
{
  const std::string      s = "x";
  const std::string_view v = s;
  std::istringstream     is{s};
  char const*            p = s.data();

  static_assert(std::same_as<
                decltype(json::deserializer{s, std::cw<json_lenient>}),
                json::deserializer<str_in, json_lenient>>);
  static_assert(std::same_as<
                decltype(json::deserializer{v, std::cw<json_lenient>}),
                json::deserializer<view_in, json_lenient>>);
  static_assert(std::same_as<
                decltype(json::deserializer{p, p + 1, std::cw<json_lenient>}),
                json::deserializer<char const*, json_lenient>>);
  static_assert(std::same_as<
                decltype(json::deserializer{is, std::cw<json_lenient>}),
                json::deserializer<std::istreambuf_iterator<char>, json_lenient>>);
  static_assert(std::same_as<
                decltype(yaml::deserializer{s, std::cw<yaml_keep>}),
                yaml::deserializer<str_in, yaml_keep>>);
  static_assert(std::same_as<
                decltype(yaml::deserializer{v, std::cw<yaml_keep>}),
                yaml::deserializer<view_in, yaml_keep>>);
  static_assert(std::same_as<
                decltype(yaml::deserializer{p, p + 1, std::cw<yaml_keep>}),
                yaml::deserializer<char const*, yaml_keep>>);
  static_assert(std::same_as<
                decltype(toml::deserializer{s, std::cw<toml_read_keep>}),
                toml::deserializer<str_in, toml_read_keep>>);
  static_assert(std::same_as<
                decltype(toml::deserializer{v, std::cw<toml_read_keep>}),
                toml::deserializer<view_in, toml_read_keep>>);
  static_assert(std::same_as<
                decltype(toml::deserializer{p, p + 1, std::cw<toml_read_keep>}),
                toml::deserializer<char const*, toml_read_keep>>);
  static_assert(
      std::same_as<
          decltype(xml::deserializer{s, std::cw<xml_keep>}), xml::deserializer<str_in, xml_keep>>);
  static_assert(
      std::same_as<
          decltype(xml::deserializer{v, std::cw<xml_keep>}), xml::deserializer<view_in, xml_keep>>);
  static_assert(std::same_as<
                decltype(xml::deserializer{p, p + 1, std::cw<xml_keep>}),
                xml::deserializer<char const*, xml_keep>>);
  static_assert(decltype(json::deserializer{s, std::cw<json_lenient>})::read_opts.trailing_commas);
}

TEST_CASE("reflex::serde options: a tag that disagrees with the explicit options is rejected")
{
  using lenient_json = json::deserializer<char const*, json::read_options{.comments = true}>;
  using pretty_json  = json::serializer<str_out, json::options{.indent = 2}>;
  static_assert(not std::constructible_from<
                lenient_json, char const*, char const*, decltype(std::cw<json::read_options{}>)>);
  static_assert(std::constructible_from<
                lenient_json, char const*, char const*,
                decltype(std::cw<json::read_options{.comments = true}>)>);
  static_assert(
      not std::constructible_from<pretty_json, std::string&, decltype(std::cw<json::options{}>)>);
  static_assert(std::constructible_from<
                pretty_json, std::string&, decltype(std::cw<json::options{.indent = 2}>)>);
  static_assert(not std::constructible_from<
                xml::serializer<str_out>, std::string&, decltype(std::cw<json::options{}>)>);
}

TEST_CASE("reflex::serde options: json comments with indent instantiate")
{
  static_assert(
      sizeof(json::serializer<str_out, json::options{.indent = 2, .comments = true}>) > 0);
}

TEST_CASE("reflex::serde options: mmap_input_stream deduces through the contiguous range guide")
{
  const std::filesystem::path path = "test-serde-options-mmap.json";
  {
    std::ofstream out{path, std::ios::binary};
    json::serializer{out}.dump(sample);
  }
  {
    mmap_input_stream  in{path};
    json::deserializer de{in, std::cw<json_lenient>};
    static_assert(std::same_as<decltype(de), json::deserializer<char const*, json_lenient>>);
    CHECK(de.load<opt_point>() == sample);
  }
  std::filesystem::remove(path);
}

TEST_CASE("reflex::serde options: non-default options write and read today's bytes")
{
  SUBCASE("json")
  {
    std::string plain;
    std::string tagged;
    json::serializer{plain}.dump(sample);
    json::serializer{tagged, std::cw<json_pretty>}.dump(sample);
    CHECK_EQ(tagged, plain);
    CHECK(json::deserializer{tagged, std::cw<json_lenient>}.load<opt_point>() == sample);
  }
  SUBCASE("yaml")
  {
    std::string plain;
    std::string tagged;
    yaml::serializer{plain}.dump(sample);
    yaml::serializer{tagged, std::cw<yaml_wide>}.dump(sample);
    CHECK_EQ(tagged, plain);
    CHECK(yaml::deserializer{tagged, std::cw<yaml_keep>}.load<opt_point>() == sample);
  }
  SUBCASE("toml")
  {
    std::string plain;
    std::string tagged;
    toml::serializer{plain}.dump(sample);
    toml::serializer{tagged, std::cw<toml_keep>}.dump(sample);
    CHECK_EQ(tagged, plain);
    CHECK(toml::deserializer{tagged, std::cw<toml_read_keep>}.load<opt_point>() == sample);
  }
  SUBCASE("xml")
  {
    std::string plain;
    std::string tagged;
    xml::serializer{plain}.dump(sample);
    xml::serializer{tagged, std::cw<xml_pretty>}.dump(sample);
    CHECK_EQ(tagged, plain);
    CHECK(xml::deserializer{tagged, std::cw<xml_keep>}.load<opt_point>() == sample);
  }
  SUBCASE("a stream sink")
  {
    std::ostringstream plain;
    std::ostringstream tagged;
    json::serializer{plain}.dump(sample);
    json::serializer{tagged, std::cw<json_pretty>}.dump(sample);
    CHECK_EQ(tagged.str(), plain.str());
    std::istringstream in{tagged.str()};
    CHECK(json::deserializer{in, std::cw<json_lenient>}.load<opt_point>() == sample);
  }
}
