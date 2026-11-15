#include <doctest/doctest.h>

import reflex.serde.bson;
import reflex.serde.csv;
import reflex.serde.json;
import reflex.serde.toml;
import reflex.serde.xml;
import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;

namespace reflex::serde::json
{
template <typename OutputIt, auto Opts>
OutputIt
    tag_invoke(tag_t<serde::serialize>, serializer<OutputIt, Opts>& ser, bson::datetime const& dt)
{
  return tag_invoke(tag_t<serde::serialize>{}, ser, std::format("{}", dt));
}
}

namespace reflex::serde::yaml
{
template <typename OutputIt, auto Opts>
OutputIt
    tag_invoke(tag_t<serde::serialize>, serializer<OutputIt, Opts>& ser, bson::datetime const& dt)
{
  return tag_invoke(tag_t<serde::serialize>{}, ser, std::format("{}", dt));
}
}

namespace reflex::serde::toml
{
template <typename OutputIt, auto Opts>
OutputIt
    tag_invoke(tag_t<serde::serialize>, serializer<OutputIt, Opts>& ser, bson::datetime const& dt)
{
  return tag_invoke(tag_t<serde::serialize>{}, ser, std::format("{}", dt));
}
}

namespace
{
using bytes = std::vector<std::byte>;

struct holder
{
  bson::binary v;
};

bytes text_bytes(std::string_view text)
{
  bytes out;
  for(char c : text)
  {
    out.push_back(static_cast<std::byte>(c));
  }
  return out;
}

bson::binary dead(int subtype)
{
  return {
      .subtype = static_cast<std::byte>(subtype), .data = {std::byte{0xDE}, std::byte{0xAD}}
  };
}

template <typename T> std::string json_of(T const& value)
{
  std::string      out;
  json::serializer ser{out};
  ser.dump(value);
  return out;
}

template <typename T> std::string yaml_of(T const& value)
{
  std::string      out;
  yaml::serializer ser{out};
  ser.dump(value);
  return out;
}

template <typename T> std::string toml_of(T const& value)
{
  std::string      out;
  toml::serializer ser{out};
  ser.dump(value);
  return out;
}

using string_out = std::back_insert_iterator<std::string>;
using bytes_out  = std::back_insert_iterator<bytes>;

static_assert(serde::detail::ext_json_serializer_c<json::serializer<string_out>>);
static_assert(
    serde::detail::ext_json_serializer_c<json::serializer<string_out, json::options{.indent = 2}>>);
static_assert(serde::detail::ext_json_serializer_c<yaml::serializer<string_out>>);
static_assert(serde::detail::ext_json_serializer_c<toml::serializer<string_out>>);
static_assert(not serde::detail::ext_json_serializer_c<xml::serializer<string_out>>);
static_assert(not serde::detail::ext_json_serializer_c<csv::serializer<string_out>>);
static_assert(not serde::detail::ext_json_serializer_c<bson::serializer<bytes_out>>);
}

TEST_CASE("reflex::serde::bson: binary is written as Extended JSON $binary in JSON")
{
  CHECK_EQ(json_of(dead(0x00)), R"({"$binary":{"base64":"3q0=","subType":"00"}})");
  CHECK_EQ(json_of(holder{dead(0x04)}), R"({"v":{"$binary":{"base64":"3q0=","subType":"04"}}})");
  CHECK_EQ(json_of(holder{dead(0x80)}), R"({"v":{"$binary":{"base64":"3q0=","subType":"80"}}})");
}

TEST_CASE("reflex::serde::bson: binary is written as Extended JSON $binary in YAML")
{
  CHECK_EQ(yaml_of(holder{dead(0x00)}), "v:\n  $binary:\n    base64: 3q0=\n    subType: '00'");
  CHECK_EQ(yaml_of(holder{dead(0x04)}), "v:\n  $binary:\n    base64: 3q0=\n    subType: '04'");
}

TEST_CASE("reflex::serde::bson: binary is written as Extended JSON $binary in TOML")
{
  CHECK_EQ(
      toml_of(holder{dead(0x00)}), "[v]\n[v.\"$binary\"]\nbase64 = \"3q0=\"\nsubType = \"00\"");
  CHECK_EQ(
      toml_of(holder{dead(0x04)}), "[v]\n[v.\"$binary\"]\nbase64 = \"3q0=\"\nsubType = \"04\"");
}

TEST_CASE("reflex::serde::bson: binary inside bson::value is written as $binary")
{
  const bson::value doc = bson::object{
      {"b",    dead(0x04)                          },
      {"list", bson::array{bson::value{dead(0x00)}}},
  };
  CHECK_EQ(
      json_of(doc), R"({"b":{"$binary":{"base64":"3q0=","subType":"04"}},)"
                    R"("list":[{"$binary":{"base64":"3q0=","subType":"00"}}]})");
  CHECK_EQ(
      yaml_of(doc), "b:\n  $binary:\n    base64: 3q0=\n    subType: '04'\n"
                    "list:\n  - $binary:\n      base64: 3q0=\n      subType: '00'");
  CHECK_EQ(
      toml_of(doc), "b = { \"$binary\" = { base64 = \"3q0=\", subType = \"04\" } }\n"
                    "list = [{ \"$binary\" = { base64 = \"3q0=\", subType = \"00\" } }]");
}

TEST_CASE("reflex::serde::bson: binary two objects deep in bson::value is written as $binary")
{
  const bson::value doc = bson::object{
      {"outer", bson::object{{"inner", dead(0x04)}}},
  };
  CHECK_EQ(json_of(doc), R"({"outer":{"inner":{"$binary":{"base64":"3q0=","subType":"04"}}}})");
  CHECK_EQ(yaml_of(doc), "outer:\n  inner:\n    $binary:\n      base64: 3q0=\n      subType: '04'");
  CHECK_EQ(
      toml_of(doc), "[outer]\ninner = { \"$binary\" = { base64 = \"3q0=\", subType = \"04\" } }");
}

TEST_CASE("reflex::serde::bson: binary base64 matches the RFC 4648 test vectors")
{
  const std::array<std::pair<std::string_view, std::string_view>, 7> vectors{
      {
       {"", ""},
       {"f", "Zg=="},
       {"fo", "Zm8="},
       {"foo", "Zm9v"},
       {"foob", "Zm9vYg=="},
       {"fooba", "Zm9vYmE="},
       {"foobar", "Zm9vYmFy"},
       }
  };
  for(auto const& [in, out] : vectors)
  {
    CAPTURE(in);
    CHECK_EQ(
        json_of(bson::binary{.data = text_bytes(in)}),
        std::format(R"({{"$binary":{{"base64":"{}","subType":"00"}}}})", out));
  }
  CHECK_EQ(
      json_of(
          bson::binary{
              .subtype = std::byte{0xFF},
              .data    = {std::byte{0xFB}, std::byte{0xFF}, std::byte{0xBF}}
  }),
      R"({"$binary":{"base64":"+/+/","subType":"ff"}})");
}

TEST_CASE("reflex::serde::bson: a root binary still serializes to BSON bytes")
{
  bytes            out;
  bson::serializer ser{out};
  serde::serialize(ser, dead(0x04));

  const bytes expected{std::byte{0x13}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                       std::byte{0x05}, std::byte{'v'},  std::byte{'a'},  std::byte{'l'},
                       std::byte{'u'},  std::byte{'e'},  std::byte{0x00}, std::byte{0x02},
                       std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x04},
                       std::byte{0xDE}, std::byte{0xAD}, std::byte{0x00}};
  CHECK(out == expected);
}
