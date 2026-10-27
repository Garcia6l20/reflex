#include <doctest/doctest.h>

import reflex.serde.json;
import reflex.serde.xml;

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

const uk_config uk_expected{1, {8080, "h"}, "z"};

constexpr json::read_options json_skip{.unknown_keys = unknown_keys::skip};

template <typename T> T json_skip_load(std::string const& text)
{
  json::deserializer de{text, std::cw<json_skip>};
  return de.template load<T>();
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
