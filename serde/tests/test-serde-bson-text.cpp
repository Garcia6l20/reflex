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

bson::object_id oid()
{
  bson::object_id id;
  for(std::size_t i = 0; i < id.bytes.size(); ++i)
  {
    id.bytes[i] = static_cast<std::byte>(0x50 + i * 17);
  }
  return id;
}

constexpr std::string_view oid_hex = "5061728394a5b6c7d8e9fa0b";

bytes scope_x7()
{
  return {std::byte{0x0C}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
          std::byte{0x10}, std::byte{'x'},  std::byte{0x00}, std::byte{0x07},
          std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
}

bson::timestamp ts()
{
  return {.increment = 2, .seconds = 1700000000};
}

bson::regex re()
{
  return {.pattern = R"(^a"b\d)", .options = "im"};
}

bson::code_w_scope cws()
{
  return {.code = "f(x)", .scope = scope_x7()};
}

bson::db_pointer dbp()
{
  return {.ref = "db.coll", .id = oid()};
}

struct extended_holder
{
  bson::object_id    oid;
  bson::timestamp    ts;
  bson::regex        re;
  bson::javascript   js;
  bson::code_w_scope cws;
  bson::symbol       sym;
  bson::db_pointer   dbp;
  bson::min_key      min;
  bson::max_key      max;
  bson::undefined    u;
  int                after = 3;
};

extended_holder all_extended()
{
  return {
      .oid = oid(),
      .ts  = ts(),
      .re  = re(),
      .js  = {"g()"},
      .cws = cws(),
      .sym = {"s"},
      .dbp = dbp(),
  };
}

bson::object all_extended_object()
{
  return {
      {"oid", oid()                  },
      {"ts",  ts()                   },
      {"re",  re()                   },
      {"js",  bson::javascript{"g()"}},
      {"cws", cws()                  },
      {"sym", bson::symbol{"s"}      },
      {"dbp", dbp()                  },
      {"min", bson::min_key{}        },
      {"max", bson::max_key{}        },
      {"u",   bson::undefined{}      },
  };
}

std::map<std::string, std::string> extended_json()
{
  const auto oid_json = std::format(R"({{"$oid":"{}"}})", oid_hex);
  return {
      {"oid", oid_json},
      {"ts", R"j({"$timestamp":{"t":1700000000,"i":2}})j"},
      {"re", R"j({"$regularExpression":{"pattern":"^a\"b\\d","options":"im"}})j"},
      {"js", R"j({"$code":"g()"})j"},
      {"cws", R"j({"$code":"f(x)","$scope":{"x":7}})j"},
      {"sym", R"j({"$symbol":"s"})j"},
      {"dbp", std::format(R"({{"$dbPointer":{{"$ref":"db.coll","$id":{}}}}})", oid_json)},
      {"min", R"j({"$minKey":1})j"},
      {"max", R"j({"$maxKey":1})j"},
      {"u", R"j({"$undefined":true})j"},
  };
}

std::string json_members(std::span<std::string const> keys)
{
  const auto  values = extended_json();
  std::string out;
  for(auto const& key : keys)
  {
    if(not out.empty())
    {
      out += ',';
    }
    out += std::format(R"("{}":{})", key, values.at(key));
  }
  return out;
}

struct ext_vector_holder
{
  std::vector<bson::object_id> ids;
  std::vector<bson::binary>    bins;
};

struct ext_deep_inner
{
  bson::binary    bin;
  bson::timestamp ts;
};

struct ext_deep_holder
{
  ext_deep_inner nested;
};

std::map<std::string, std::string> extended_toml()
{
  const auto oid_toml = std::format(R"({{ "$oid" = "{}" }})", oid_hex);
  return {
      {"bin", R"t({ "$binary" = { base64 = "3q0=", subType = "04" } })t"},
      {"oid", oid_toml},
      {"ts", R"t({ "$timestamp" = { t = 1700000000, i = 2 } })t"},
      {"re", R"t({ "$regularExpression" = { pattern = '^a"b\d', options = "im" } })t"},
      {"js", R"t({ "$code" = "g()" })t"},
      {"cws", R"t({ "$code" = "f(x)", "$scope" = { x = 7 } })t"},
      {"sym", R"t({ "$symbol" = "s" })t"},
      {"dbp",
       std::format(R"({{ "$dbPointer" = {{ "$ref" = "db.coll", "$id" = {} }} }})", oid_toml)},
      {"min", R"t({ "$minKey" = 1 })t"},
      {"max", R"t({ "$maxKey" = 1 })t"},
      {"u", R"t({ "$undefined" = true })t"},
  };
}

std::map<std::string, std::string> extended_toml_root()
{
  return {
      {"bin", R"t("$binary" = { base64 = "3q0=", subType = "04" })t"},
      {"oid", std::format(R"("$oid" = "{}")", oid_hex)},
      {"ts", R"t("$timestamp" = { t = 1700000000, i = 2 })t"},
      {"re", R"t("$regularExpression" = { pattern = '^a"b\d', options = "im" })t"},
      {"js", R"t("$code" = "g()")t"},
      {"cws", "\"$code\" = \"f(x)\"\n\"$scope\" = { x = 7 }"},
      {"sym", R"t("$symbol" = "s")t"},
      {"dbp",
       std::format(
           R"("$dbPointer" = {{ "$ref" = "db.coll", "$id" = {{ "$oid" = "{}" }} }})", oid_hex)},
      {"min", R"t("$minKey" = 1)t"},
      {"max", R"t("$maxKey" = 1)t"},
      {"u", R"t("$undefined" = true)t"},
  };
}

std::string toml_lines(std::span<std::string const> keys)
{
  const auto  values = extended_toml();
  std::string out;
  for(auto const& key : keys)
  {
    if(not out.empty())
    {
      out += '\n';
    }
    out += std::format("{} = {}", key, values.at(key));
  }
  return out;
}

bson::object all_text_object()
{
  auto object   = all_extended_object();
  object["bin"] = dead(0x04);
  return object;
}

template <typename T> bytes bson_root_of(T const& value)
{
  bytes            out;
  bson::serializer ser{out};
  serde::serialize(ser, value);
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
      toml_of(holder{dead(0x00)}), "v = { \"$binary\" = { base64 = \"3q0=\", subType = \"00\" } }");
  CHECK_EQ(
      toml_of(holder{dead(0x04)}), "v = { \"$binary\" = { base64 = \"3q0=\", subType = \"04\" } }");
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

TEST_CASE("reflex::serde::bson: each extended type is written as Extended JSON v2 in JSON")
{
  const auto values = extended_json();
  const auto object = all_extended_object();
  for(auto const& [key, value] : object)
  {
    CAPTURE(key);
    CHECK_EQ(json_of(value), values.at(key));
    reflex::visit(
        [&](auto const& alternative) { CHECK_EQ(json_of(alternative), values.at(key)); }, value);
  }
}

TEST_CASE("reflex::serde::bson: extended types nested two levels in bson::value")
{
  const std::array<std::string, 10> keys{"cws", "dbp", "js",  "max", "min",
                                         "oid", "re",  "sym", "ts",  "u"};
  const auto                        members = json_members(keys);

  const bson::value in_objects = bson::object{
      {"outer", bson::object{{"inner", all_extended_object()}}}
  };
  CHECK_EQ(json_of(in_objects), std::format(R"({{"outer":{{"inner":{{{}}}}}}})", members));

  bson::array inner;
  for(auto const& [key, value] : all_extended_object())
  {
    inner.push_back(value);
  }
  const bson::value in_arrays = bson::array{bson::value{bson::array{bson::value{inner}}}};
  std::string       elements;
  for(auto const& [key, value] : extended_json())
  {
    elements += elements.empty() ? "" : ",";
    elements += value;
  }
  CHECK_EQ(json_of(in_arrays), std::format("[[[{}]]]", elements));
}

TEST_CASE("reflex::serde::bson: extended types as typed aggregate members")
{
  const std::array<std::string, 10> keys{"oid", "ts",  "re",  "js",  "cws",
                                         "sym", "dbp", "min", "max", "u"};
  CHECK_EQ(json_of(all_extended()), std::format(R"({{{},"after":3}})", json_members(keys)));
}

TEST_CASE("reflex::serde::bson: extended types are written as Extended JSON v2 in YAML")
{
  const bson::value doc = bson::object{
      {"cws", cws()},
      {"oid", oid()},
      {"re",  re() },
      {"ts",  ts() },
  };
  CHECK_EQ(
      yaml_of(doc), std::format(
                        "cws:\n  $code: f(x)\n  $scope:\n    x: 7\n"
                        "oid:\n  $oid: {}\n"
                        "re:\n  $regularExpression:\n    pattern: ^a\"b\\d\n    options: im\n"
                        "ts:\n  $timestamp:\n    t: 1700000000\n    i: 2",
                        oid_hex));
  CHECK_EQ(yaml_of(oid()), std::format("$oid: {}", oid_hex));
  CHECK_EQ(yaml_of(ts()), "$timestamp:\n  t: 1700000000\n  i: 2");
  CHECK_EQ(yaml_of(cws()), "$code: f(x)\n$scope:\n  x: 7");

  const bson::value keys = bson::object{
      {"max", bson::max_key{}  },
      {"min", bson::min_key{}  },
      {"u",   bson::undefined{}},
  };
  CHECK_EQ(yaml_of(keys), "max:\n  $maxKey: 1\nmin:\n  $minKey: 1\nu:\n  $undefined: true");
  CHECK_EQ(
      yaml_of(bson::array{bson::value{bson::min_key{}}, bson::value{bson::undefined{}}}),
      "- $minKey: 1\n- $undefined: true");
}

TEST_CASE("reflex::serde::bson: extended types are written as Extended JSON v2 in TOML")
{
  const bson::value doc = bson::object{
      {"cws", cws()},
      {"oid", oid()},
      {"re",  re() },
      {"ts",  ts() },
  };
  CHECK_EQ(
      toml_of(doc), std::format(
                        "cws = {{ \"$code\" = \"f(x)\", \"$scope\" = {{ x = 7 }} }}\n"
                        "oid = {{ \"$oid\" = \"{}\" }}\n"
                        "re = {{ \"$regularExpression\" = {{ pattern = '^a\"b\\d', "
                        "options = \"im\" }} }}\n"
                        "ts = {{ \"$timestamp\" = {{ t = 1700000000, i = 2 }} }}",
                        oid_hex));
  CHECK_EQ(toml_of(oid()), std::format("\"$oid\" = \"{}\"", oid_hex));
  CHECK_EQ(toml_of(ts()), "\"$timestamp\" = { t = 1700000000, i = 2 }");
  CHECK_EQ(toml_of(cws()), "\"$code\" = \"f(x)\"\n\"$scope\" = { x = 7 }");
}

TEST_CASE("reflex::serde::bson: TOML writes every Extended JSON document inline in every position")
{
  const auto               inline_forms = extended_toml();
  const auto               root_forms   = extended_toml_root();
  const auto               object       = all_text_object();
  std::vector<std::string> sorted_keys;
  std::string              elements;
  for(auto const& [key, value] : object)
  {
    CAPTURE(key);
    CHECK_EQ(toml_of(value), root_forms.at(key));
    reflex::visit(
        [&](auto const& alternative) { CHECK_EQ(toml_of(alternative), root_forms.at(key)); },
        value);
    sorted_keys.push_back(key);
    elements += elements.empty() ? "" : ", ";
    elements += inline_forms.at(key);
  }

  CHECK_EQ(toml_of(bson::value{object}), toml_lines(sorted_keys));

  const std::array<std::string, 10> member_keys{"oid", "ts",  "re",  "js",  "cws",
                                                "sym", "dbp", "min", "max", "u"};
  CHECK_EQ(toml_of(all_extended()), toml_lines(member_keys) + "\nafter = 3");

  bson::array list;
  for(auto const& [key, value] : object)
  {
    list.push_back(value);
  }
  const bson::value in_array = bson::object{
      {"list", list}
  };
  CHECK_EQ(toml_of(in_array), std::format("list = [{}]", elements));

  const bson::value two_deep = bson::object{
      {"outer", bson::object{{"inner", object}}}
  };
  CHECK_EQ(toml_of(two_deep), "[outer]\n[outer.inner]\n" + toml_lines(sorted_keys));

  const std::array<std::string, 2> deep_keys{"bin", "ts"};
  ext_deep_holder                  deep;
  deep.nested.bin = dead(0x04);
  deep.nested.ts  = ts();
  CHECK_EQ(toml_of(deep), "[nested]\n" + toml_lines(deep_keys));

  ext_vector_holder vectors;
  vectors.ids  = {oid(), oid()};
  vectors.bins = {dead(0x04)};
  CHECK_EQ(
      toml_of(vectors),
      std::format(
          "ids = [{0}, {0}]\nbins = [{1}]", inline_forms.at("oid"), inline_forms.at("bin")));
}

TEST_CASE("reflex::serde::bson: a malformed code_w_scope scope throws before its own text")
{
  const std::array<bytes, 3> scopes{
      bytes{},
      bytes{std::byte{0x06}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}},
      bytes{
            std::byte{0x06}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x42},
            std::byte{0x00}},
  };
  for(auto const& scope : scopes)
  {
    CAPTURE(scope.size());
    const bson::code_w_scope value{.code = "f", .scope = scope};
    std::string              out;
    json::serializer         ser{out};
    CHECK_THROWS(ser.dump(value));
    CHECK(out.empty());

    std::string      yaml_out;
    yaml::serializer yaml_ser{yaml_out};
    CHECK_THROWS(yaml_ser.dump(value));
    CHECK(yaml_out.empty());

    std::string      toml_out;
    toml::serializer toml_ser{toml_out};
    CHECK_THROWS(toml_ser.dump(value));
    CHECK(toml_out.empty());
  }
  CHECK_THROWS_WITH_AS(
      json_of(bson::code_w_scope{.code = "f", .scope = {}}),
      "BSON code with scope needs an encoded scope document", std::invalid_argument);
}

TEST_CASE("reflex::serde::bson: a root extended type still serializes to BSON bytes")
{
  for(auto const& [key, value] : all_extended_object())
  {
    CAPTURE(key);
    const bytes expected = bson_root_of(
        bson::object{
            {"value", value}
    });
    reflex::visit(
        [&](auto const& alternative) { CHECK(bson_root_of(alternative) == expected); }, value);
  }
}

TEST_CASE("reflex::serde::bson: regex options are written in alphabetical order")
{
  const bson::regex unsorted{.pattern = "a", .options = "xmi"};

  CHECK_EQ(
      bson_root_of(unsorted), text_bytes(
                                  std::string_view{
                                      "\x12\0\0\0\x0b"
                                      "value\0"
                                      "a\0"
                                      "imx\0"
                                      "\0",
                                      18}));
  CHECK_EQ(json_of(unsorted), R"({"$regularExpression":{"pattern":"a","options":"imx"}})");
  CHECK_EQ(yaml_of(unsorted), "$regularExpression:\n  pattern: a\n  options: imx");
  CHECK_EQ(toml_of(unsorted), "\"$regularExpression\" = { pattern = \"a\", options = \"imx\" }");
  CHECK_EQ(
      json_of(bson::regex{"a", "mim"}), R"({"$regularExpression":{"pattern":"a","options":"im"}})");
  CHECK_EQ(unsorted.options, "xmi");
}

TEST_CASE("reflex::serde::bson: regex options keep each letter once and reject foreign letters")
{
  CHECK_EQ(
      bson_root_of(bson::regex{"a", "mim"}), text_bytes(
                                                 std::string_view{
                                                     "\x11\0\0\0\x0b"
                                                     "value\0"
                                                     "a\0"
                                                     "im\0"
                                                     "\0",
                                                     17}));
  constexpr std::string_view message =
      "BSON regex options may only hold the letters i, l, m, s, u, x";
  for(std::string_view options : {"g", "\xc3\xa9m"})
  {
    CAPTURE(options);
    const bson::regex value{.pattern = "a", .options = std::string{options}};
    CHECK_THROWS_WITH_AS(std::ignore = bson_root_of(value), message.data(), std::invalid_argument);
    CHECK_THROWS_WITH_AS(std::ignore = json_of(value), message.data(), std::invalid_argument);
  }
}
