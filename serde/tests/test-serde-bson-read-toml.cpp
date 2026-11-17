#include <doctest/doctest.h>

import reflex.serde.json;
import reflex.serde.toml;
import reflex.serde.bson;

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

template <typename It, auto Opts>
bson::datetime tag_invoke(
    tag_t<serde::deserialize>,
    deserializer<It, Opts>& de,
    std::type_identity<bson::datetime>)
{
  auto s = tag_invoke(tag_t<serde::deserialize>{}, de, std::type_identity<std::string>{});
  return reflex::parse_or_throw<bson::datetime>(s);
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

template <typename It, auto Opts>
bson::datetime tag_invoke(
    tag_t<serde::deserialize>,
    deserializer<It, Opts>& de,
    std::type_identity<bson::datetime>)
{
  auto s = tag_invoke(tag_t<serde::deserialize>{}, de, std::type_identity<std::string>{});
  return reflex::parse_or_throw<bson::datetime>(s);
}
}

namespace
{
using bytes = std::vector<std::byte>;

constexpr std::string_view oid_hex = "5061728394a5b6c7d8e9fa0b";

bson::binary dead(int subtype)
{
  return {
      .subtype = static_cast<std::byte>(subtype), .data = {std::byte{0xDE}, std::byte{0xAD}}
  };
}

bson::object_id oid()
{
  bson::object_id id;
  for(std::size_t i = 0; i < id.bytes.size(); ++i)
  {
    unsigned byte = 0;
    std::from_chars(oid_hex.data() + 2 * i, oid_hex.data() + 2 * i + 2, byte, 16);
    id.bytes[i] = static_cast<std::byte>(byte);
  }
  return id;
}

bytes scope_x7()
{
  return {std::byte{0x0C}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
          std::byte{0x10}, std::byte{'x'},  std::byte{0x00}, std::byte{0x07},
          std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
}

bson::code_w_scope cws()
{
  return {.code = "f(x)", .scope = scope_x7()};
}

bson::timestamp ts()
{
  return {.increment = 2, .seconds = 1700000000};
}

bson::regex re()
{
  return {.pattern = R"(^a"b\d)", .options = "im"};
}

bson::object all_types()
{
  return {
      {"bin",  dead(0x00)                                     },
      {"bin4", dead(0x04)                                     },
      {"bin8", dead(0x80)                                     },
      {"oid",  oid()                                          },
      {"ts",   ts()                                           },
      {"re",   re()                                           },
      {"js",   bson::javascript{"g()"}                        },
      {"cws",  cws()                                          },
      {"sym",  bson::symbol{"s"}                              },
      {"dbp",  bson::db_pointer{.ref = "db.coll", .id = oid()}},
      {"min",  bson::min_key{}                                },
      {"max",  bson::max_key{}                                },
      {"u",    bson::undefined{}                              },
  };
}

std::map<std::string, std::string> written_toml()
{
  const auto oid_toml = std::format(R"({{ "$oid" = "{}" }})", oid_hex);
  return {
      {"bin4", R"t({ "$binary" = { base64 = "3q0=", subType = "04" } })t"},
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

std::map<std::string, std::string> written_toml_root()
{
  return {
      {"bin4", R"t("$binary" = { base64 = "3q0=", subType = "04" })t"},
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

std::string toml_of(bson::value const& value)
{
  std::string      out;
  toml::serializer ser{out};
  ser.dump(value);
  return out;
}

bson::value read_toml(std::string text)
{
  return toml::deserializer{text}.load<bson::value>();
}

void check_round_trip(bson::value const& original)
{
  const auto text = toml_of(original);
  CAPTURE(text);
  const auto read = read_toml(text);
  CHECK(read.index() == original.index());
  CHECK(read == original);
  CHECK_EQ(toml_of(read), text);
}

void check_throws(std::string text, std::string const& message)
{
  CAPTURE(text);
  CHECK_THROWS_WITH_AS(read_toml(text), message.c_str(), std::runtime_error);
}

std::string under(std::string_view key, std::string_view message)
{
  return std::format("TOML: key '{}': {}", key, message);
}

struct read_toml_member
{
  bson::value v;
  int         n = 0;
};

bson::value bson_doc(std::string key, bson::value value)
{
  return bson::value{bson::object{{std::move(key), std::move(value)}}};
}
}

TEST_CASE("reflex::serde::bson: TOML reads each written type back into bson::value")
{
  for(auto const& [key, value] : all_types())
  {
    CAPTURE(key);
    check_round_trip(value);
    check_round_trip(bson_doc("k", value));
    check_round_trip(
        bson::object{
            {"outer", bson::object{{"inner", value}, {"after", 3}}}
    });
    check_round_trip(bson_doc("a", bson::array{value, value}));
    check_round_trip(bson_doc("a", bson::array{bson::value{bson::array{value, value}}}));
  }
  check_round_trip(all_types());
}

TEST_CASE("reflex::serde::bson: TOML reads the pinned writer output to the original values")
{
  const auto objects = all_types();
  for(auto const& [key, text] : written_toml())
  {
    CAPTURE(key);
    auto const& expected = objects.at(key);
    auto        member   = read_toml(std::format("k = {}", text));
    CHECK(member["k"].index() == expected.index());
    CHECK(member["k"] == expected);
    const auto root = read_toml(written_toml_root().at(key));
    CHECK(root.index() == expected.index());
    CHECK(root == expected);
  }
}

TEST_CASE("reflex::serde::bson: TOML reads type wrappers written as header tables")
{
  auto binary = read_toml("[a.\"$binary\"]\nbase64 = \"3q0=\"\nsubType = \"04\"");
  REQUIRE(binary["a"].is<bson::binary>());
  CHECK(binary["a"] == bson::value{dead(0x04)});

  auto stamp = read_toml("[a.\"$timestamp\"]\nt = 1\ni = 2\n[b]\ny = 1");
  REQUIRE(stamp["a"].is<bson::timestamp>());
  CHECK(
      stamp["a"]
      == bson::value{
          bson::timestamp{.increment = 2, .seconds = 1}
  });
  REQUIRE(stamp["b"].is_object());
  CHECK(stamp["b"]["y"] == 1);

  check_throws(
      "[a.\"$timestamp\"]\nt = 1\ni = 2\n[b]\ny = 1\n[a.\"$timestamp\".z]\nw = 0",
      "Extended JSON: unexpected key 'z' in a $timestamp document");

  auto id = read_toml(std::format("[a]\n\"$oid\" = \"{}\"", oid_hex));
  REQUIRE(id["a"].is<bson::object_id>());
  CHECK(id["a"] == bson::value{oid()});

  auto dotted = read_toml(std::format("a.\"$oid\" = \"{}\"", oid_hex));
  CHECK(dotted["a"] == bson::value{oid()});

  auto in_inline = read_toml(std::format("x = {{ a.\"$oid\" = \"{}\" }}", oid_hex));
  CHECK(in_inline["x"]["a"] == bson::value{oid()});

  auto later = read_toml(std::format("[t]\ny = 1\n[u]\nz = 2\n[t.w]\n\"$oid\" = \"{}\"", oid_hex));
  CHECK(later["t"]["w"] == bson::value{oid()});

  auto deep_scope = read_toml(
      "a = { b = { c = { \"$code\" = \"f(x)\", \"$scope\" = { x = { \"$numberInt\" = \"7\" } } } } "
      "}");
  CHECK(deep_scope["a"]["b"]["c"] == bson::value{cws()});

  auto dotted_body = read_toml("x = { y = { \"$date\".\"$numberLong\" = \"5\" } }");
  REQUIRE(dotted_body["x"]["y"].is<bson::datetime>());
  CHECK(dotted_body["x"]["y"].as<bson::datetime>().millis_since_epoch == 5);

  auto tables = read_toml("[[a]]\n\"$minKey\" = 1\n[[a]]\n\"$maxKey\" = 1");
  CHECK(
      tables["a"]
      == bson::value{
          bson::array{bson::value{bson::min_key{}}, bson::value{bson::max_key{}}}
  });

  auto pointer = read_toml(
      std::format(
          "[p.\"$dbPointer\"]\n\"$ref\" = \"db.coll\"\n[p.\"$dbPointer\".\"$id\"]\n\"$oid\" = "
          "\"{}\"",
          oid_hex));
  CHECK(
      pointer["p"]
      == bson::value{
          bson::db_pointer{.ref = "db.coll", .id = oid()}
  });

  auto scoped = read_toml("[c]\n\"$code\" = \"f(x)\"\n[c.\"$scope\"]\nx = 7");
  CHECK(scoped["c"] == bson::value{cws()});

  const auto root = read_toml("[\"$timestamp\"]\nt = 1700000000\ni = 2");
  CHECK(root == bson::value{ts()});

  auto nested = read_toml("[a.\"$date\"]\n\"$numberLong\" = \"1700000000123\"\n[b]\ny = 1");
  REQUIRE(nested["a"].is<bson::datetime>());
  CHECK(nested["a"].as<bson::datetime>().millis_since_epoch == 1700000000123);
  CHECK(nested["b"].is_object());

  const auto long_body = read_toml("[\"$date\"]\n\"$numberLong\" = \"1700000000123\"");
  REQUIRE(long_body.is<bson::datetime>());
}

TEST_CASE("reflex::serde::bson: TOML lifts a wrapper 500 inline tables deep")
{
  constexpr int depth = 500;
  std::string   text  = "x = ";
  for(int i = 0; i < depth; ++i)
  {
    text += "{ a = ";
  }
  text += std::format("{{ \"$oid\" = \"{}\" }}", oid_hex);
  for(int i = 0; i < depth; ++i)
  {
    text += " }";
  }
  auto         read = read_toml(text);
  bson::value* node = &read["x"];
  for(int i = 0; i < depth; ++i)
  {
    REQUIRE(node->is_object());
    node = &(*node)["a"];
  }
  CHECK(*node == bson::value{oid()});
}

TEST_CASE("reflex::serde::bson: TOML timestamp and date bodies")
{
  const auto wide = read_toml("\"$timestamp\" = { t = 4000000000, i = 1 }");
  REQUIRE(wide.is<bson::timestamp>());
  CHECK(wide.as<bson::timestamp>().seconds == 4000000000u);

  for(std::string_view body : {"2023-11-14T22:13:20.123Z", "\"2023-11-14T22:13:20.123Z\""})
  {
    CAPTURE(body);
    auto date = read_toml(std::format("d = {{ \"$date\" = {} }}", body));
    REQUIRE(date["d"].is<bson::datetime>());
    CHECK(date["d"].as<bson::datetime>().millis_since_epoch == 1700000000123);
  }
}

TEST_CASE("reflex::serde::bson: TOML reads the canonical wrappers")
{
  const auto l = read_toml("\"$numberLong\" = \"5\"");
  REQUIRE(l.is<bson::int64>());
  CHECK(l.as<bson::int64>() == 5);

  auto i = read_toml("x = { \"$numberInt\" = \"-7\" }");
  REQUIRE(i["x"].is<bson::int32>());
  CHECK(i["x"].as<bson::int32>() == -7);

  auto d = read_toml("[x]\n\"$numberDouble\" = \"1.5\"");
  REQUIRE(d["x"].is<double>());
  CHECK(d["x"].as<double>() == 1.5);

  const auto m = read_toml("\"$numberDecimal\" = \"1.5\"");
  REQUIRE(m.is<bson::decimal128>());
  CHECK(m.as<bson::decimal128>() == bson::decimal128{1.5});

  const auto nan = read_toml("\"$numberDouble\" = \"NaN\"");
  REQUIRE(nan.is<double>());
  CHECK(std::isnan(nan.as<double>()));
}

TEST_CASE("reflex::serde::bson: TOML number wrappers take a string body only")
{
  const std::string bad_int = "Extended JSON: $numberInt must be a string holding a 32-bit integer";
  const std::string bad_long =
      "Extended JSON: $numberLong must be a string holding a 64-bit integer";
  const std::string bad_double = "Extended JSON: $numberDouble must be a string holding a double";
  const std::string bad_decimal =
      "Extended JSON: $numberDecimal must be a string holding a decimal128";
  check_throws("\"$numberInt\" = 5", bad_int);
  check_throws("\"$numberLong\" = 5", bad_long);
  check_throws("\"$numberDouble\" = 1.5", bad_double);
  check_throws("\"$numberDecimal\" = 1.5", bad_decimal);
  check_throws("x = { \"$numberDecimal\" = 1.5 }", bad_decimal);
  check_throws("[x]\n\"$numberLong\" = 5", bad_long);
  check_throws("a = [[{ \"$numberInt\" = 5 }]]", bad_int);
  check_throws("\"$numberDouble\" = nan", bad_double);
  check_throws("\"$numberDouble\" = inf", bad_double);
  check_throws("\"$numberDouble\" = -inf", bad_double);
  check_throws("x = { \"$numberDouble\" = +inf }", bad_double);

  auto native = read_toml("x = nan");
  REQUIRE(native["x"].is<double>());
  CHECK(std::isnan(native["x"].as<double>()));
}

TEST_CASE("reflex::serde::bson: TOML leaves tables without a reserved key as tables")
{
  const std::array<std::string_view, 4> texts{
      "[\"$set\"]\na = 1",
      "\"$type\" = 2",
      "\"$id\" = 1\n\"$ref\" = \"c\"",
      "\"$db\" = \"d\"\n\"$id\" = 1\n\"$ref\" = \"c\"",
  };
  for(auto text : texts)
  {
    CAPTURE(text);
    const auto read = read_toml(std::string{text});
    REQUIRE(read.is_object());
    CHECK_EQ(toml_of(read), text);
  }
  CHECK(read_toml("\"$set\" = { a = 1 }")["$set"]["a"] == 1);
  CHECK(read_toml("x = { \"$set\" = { a = 1 } }")["x"]["$set"]["a"] == 1);

  auto dbref =
      read_toml(std::format("[r]\n\"$ref\" = \"c\"\n\"$id\" = {{ \"$oid\" = \"{}\" }}", oid_hex));
  REQUIRE(dbref["r"].is_object());
  CHECK(dbref["r"]["$id"] == bson::value{oid()});
  CHECK(dbref["r"]["$ref"] == "c");
}

TEST_CASE("reflex::serde::bson: toml::value keeps $oid as a plain table")
{
  const auto read = toml::deserializer{std::string{"\"$oid\" = \"x\""}}.load<toml::value>();
  REQUIRE(read.is_object());
  CHECK(read.as_object().at("$oid") == "x");

  const auto nested = toml::deserializer{std::string{"[a]\n\"$oid\" = \"x\""}}.load<toml::value>();
  CHECK(nested.as_object().at("a").is_object());
}

TEST_CASE("reflex::serde::bson: TOML throws on a malformed type wrapper, naming the key")
{
  const auto extra = "Extended JSON: unexpected key 'x' in a $oid document";
  check_throws(std::format("k = {{ \"$oid\" = \"{}\", x = 1 }}", oid_hex), extra);
  check_throws(std::format("[k]\n\"$oid\" = \"{}\"\nx = 1", oid_hex), extra);

  const auto missing = "Extended JSON: a $binary document is missing 'subType'";
  check_throws("k = { \"$binary\" = { base64 = \"3q0=\" } }", missing);
  check_throws("[k.\"$binary\"]\nbase64 = \"3q0=\"", missing);

  const auto no_code = "Extended JSON: a $code document is missing '$code'";
  check_throws("k = { \"$scope\" = { x = 7 } }", no_code);
  check_throws("[k.\"$scope\"]\nx = 7", no_code);

  const auto bad_oid = "Extended JSON: $oid must be a string of 24 hex digits";
  check_throws("k = { \"$oid\" = \"zz\" }", bad_oid);
  check_throws("[k]\n\"$oid\" = \"zz\"", bad_oid);
  check_throws("[[k]]\n\"$oid\" = 1", bad_oid);

  const auto bad_sub = "Extended JSON: $binary.subType must be a string of 1 or 2 hex digits";
  check_throws("k = { \"$binary\" = { base64 = \"3q0=\", subType = 4 } }", bad_sub);
  check_throws("[k.\"$binary\"]\nbase64 = \"3q0=\"\nsubType = 4", bad_sub);

  const auto bad_t = "Extended JSON: $timestamp.t must be an integer from 0 to 4294967295";
  check_throws("k = { \"$timestamp\" = { t = -1, i = 2 } }", bad_t);
  check_throws("[k.\"$timestamp\"]\nt = 1.5\ni = 2", bad_t);

  const auto bad_id = "Extended JSON: $dbPointer.$id must be an $oid document";
  check_throws("k = { \"$dbPointer\" = { \"$ref\" = \"c\", \"$id\" = 1 } }", bad_id);
  check_throws("[k.\"$dbPointer\"]\n\"$ref\" = \"c\"\n\"$id\" = \"x\"", bad_id);

  check_throws("k = { \"$undefined\" = \"true\" }", "Extended JSON: $undefined must be true");
  check_throws("[k]\n\"$minKey\" = 2", "Extended JSON: $minKey must be 1");

  const auto bad_date = "Extended JSON: $date must be an RFC 3339 string or a $numberLong document";
  check_throws("k = { \"$date\" = 1979-05-27 }", bad_date);
  check_throws("[k.\"$date\"]\n\"$numberInt\" = \"5\"", bad_date);
}

TEST_CASE("reflex::serde::bson: TOML names the member when a typed member's wrapper is malformed")
{
  CHECK_THROWS_WITH_AS(
      toml::deserializer{std::string{"v = { \"$oid\" = \"zz\" }\nn = 1"}}.load<read_toml_member>(),
      under("v", "Extended JSON: $oid must be a string of 24 hex digits").c_str(),
      std::runtime_error);
}

namespace
{
struct read_toml_commented
{
  bson::value v;
  int         n = 0;
};

constexpr toml::read_options read_toml_keep{.comments = true};

toml::value toml_value_of(json::value const& in)
{
  if(in.is_object())
  {
    toml::object out;
    for(auto const& [key, member] : in.as_object())
    {
      out.emplace(key, toml_value_of(member));
    }
    return toml::value{std::move(out)};
  }
  if(in.is_array())
  {
    toml::array out;
    for(auto const& element : in.as_array())
    {
      out.push_back(toml_value_of(element));
    }
    return toml::value{std::move(out)};
  }
  if(in.is<json::number>())
  {
    const auto number = in.as<json::number>();
    if(number == std::trunc(number))
    {
      return toml::value{static_cast<toml::integer>(number)};
    }
    return toml::value{number};
  }
  if(in.is<json::boolean>())
  {
    return toml::value{in.as<json::boolean>()};
  }
  return toml::value{in.as<json::string>()};
}

std::string canonical_json()
{
  return R"({
    "_id": {"$oid": "57e193d7a9cc81b4027498b5"},
    "Symbol": {"$symbol": "symbol"},
    "String": "string",
    "Int32": {"$numberInt": "42"},
    "Int64": {"$numberLong": "42"},
    "Double": {"$numberDouble": "42.42"},
    "SpecialFloat": {"$numberDouble": "NaN"},
    "Decimal": {"$numberDecimal": "1234.5"},
    "Binary": {"$binary": {"base64": "yO2rw/c4TKO2jauSqRR4ow==", "subType": "04"}},
    "BinaryUserDefined": {"$binary": {"base64": "MTIz", "subType": "80"}},
    "Code": {"$code": "function() {}"},
    "CodeWithScope": {"$code": "function() {}", "$scope": {}},
    "Subdocument": {"foo": "bar"},
    "Array": [{"$numberInt": "1"}, {"$numberInt": "2"}, {"$numberInt": "3"}, {"$numberInt": "4"}, {"$numberInt": "5"}],
    "Timestamp": {"$timestamp": {"t": 42, "i": 1}},
    "RegularExpression": {"$regularExpression": {"pattern": "foo*", "options": "ix"}},
    "DatetimeEpoch": {"$date": {"$numberLong": "0"}},
    "DatetimePositive": {"$date": {"$numberLong": "253402300799999"}},
    "DatetimeNegative": {"$date": {"$numberLong": "-62135596800000"}},
    "True": true,
    "False": false,
    "DBPointer": {"$dbPointer": {"$ref": "db.collection", "$id": {"$oid": "57e193d7a9cc81b4027498b1"}}},
    "DBRef": {"$ref": "collection", "$id": {"$oid": "57e193d7a9cc81b4027498b1"}, "$db": "database"},
    "DBRefNoDB": {"$ref": "collection", "$id": {"$oid": "57fd71e96e32ab4225b723fb"}},
    "Minkey": {"$minKey": 1},
    "Maxkey": {"$maxKey": 1},
    "Undefined": {"$undefined": true}
  })";
}
}

TEST_CASE("reflex::serde::bson: TOML comments mode lifts a wrapper and keeps its comment")
{
  const auto         text = std::format("v = {{ \"$oid\" = \"{}\" }} # tv\nn = 3\n", oid_hex);
  toml::deserializer de{text, std::cw<read_toml_keep>};
  const auto [value, comments] = de.load_commented<read_toml_commented>();
  REQUIRE(value.v.is<bson::object_id>());
  CHECK(value.v == bson::value{oid()});
  CHECK(value.n == 3);
  CHECK_EQ(comments.v.trailing, "tv");
}

TEST_CASE("reflex::serde::bson: TOML reads the canonical document as JSON does")
{
  const auto  json_text = canonical_json();
  std::string toml_text;
  toml::serializer{toml_text}.dump(
      toml_value_of(json::deserializer{json_text}.load<json::value>()));
  CAPTURE(toml_text);

  const auto from_json = json::deserializer{json_text}.load<bson::value>();
  const auto from_toml = read_toml(toml_text);
  REQUIRE(from_json.is_object());
  REQUIRE(from_toml.is_object());
  auto const& expected = from_json.as_object();
  auto const& actual   = from_toml.as_object();
  CHECK(actual.size() == 27);
  CHECK(actual.size() == expected.size());
  for(auto const& [key, value] : expected)
  {
    CAPTURE(key);
    REQUIRE(actual.contains(key));
    CHECK(actual.at(key).index() == value.index());
    if(key == "SpecialFloat")
    {
      REQUIRE(actual.at(key).is<double>());
      CHECK(std::isnan(actual.at(key).as<double>()));
    }
    else
    {
      CHECK(actual.at(key) == value);
    }
  }
}

namespace
{
struct toml_typed
{
  bson::binary       bin;
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

  bool operator==(toml_typed const&) const = default;
};

struct toml_typed_list
{
  std::vector<toml_typed> items;

  bool operator==(toml_typed_list const&) const = default;
};

template <typename T> struct toml_typed_one
{
  T   v;
  int n = 0;
};

template <typename T> struct toml_shapes
{
  std::optional<T>         set;
  std::optional<T>         none;
  std::vector<T>           list;
  std::map<std::string, T> map;
  int                      tail = 5;

  bool operator==(toml_shapes const&) const = default;
};

template <typename T> toml_shapes<T> toml_shapes_of(T const& first, T const& second)
{
  return {
      .set  = first,
      .none = std::nullopt,
      .list = {first,         second        },
      .map  = {{"k1", first}, {"k2", second}},
  };
}

toml_typed toml_typed_sample()
{
  return {
      .bin = dead(0x04),
      .oid = oid(),
      .ts  = ts(),
      .re  = re(),
      .js  = {"g()"},
      .cws = cws(),
      .sym = {"s"},
      .dbp = {.ref = "db.coll", .id = oid()},
  };
}

template <typename T> std::string typed_toml_of(T const& value)
{
  std::string      out;
  toml::serializer ser{out};
  ser.dump(value);
  return out;
}

template <typename T> T read_typed_toml(std::string text)
{
  return toml::deserializer{text}.load<T>();
}

template <typename T> void check_typed_round_trip(T const& original)
{
  const auto text = typed_toml_of(original);
  CAPTURE(text);
  const auto read = read_typed_toml<T>(text);
  CHECK(read == original);
  CHECK_EQ(typed_toml_of(read), text);
}

template <typename T> void check_typed_throws(std::string text, std::string const& message)
{
  CAPTURE(text);
  CHECK_THROWS_WITH_AS(read_typed_toml<T>(text), message.c_str(), std::runtime_error);
}
}

TEST_CASE("reflex::serde::bson: TOML reads every bson type as a typed member")
{
  const auto sample = toml_typed_sample();
  const auto forms  = written_toml();
  CHECK_EQ(
      typed_toml_of(sample),
      std::format(
          "bin = {}\noid = {}\nts = {}\nre = {}\njs = {}\ncws = {}\nsym = {}\ndbp = {}\n"
          "min = {}\nmax = {}\nu = {}\nafter = 3",
          forms.at("bin4"), forms.at("oid"), forms.at("ts"), forms.at("re"), forms.at("js"),
          forms.at("cws"), forms.at("sym"), forms.at("dbp"), forms.at("min"), forms.at("max"),
          forms.at("u")));
  check_typed_round_trip(sample);
  check_typed_round_trip(
      toml_typed_list{
          .items = {sample, toml_typed{}}
  });
}

TEST_CASE("reflex::serde::bson: TOML reads bson types in optional, vector and map members")
{
  auto other     = oid();
  other.bytes[0] = std::byte{0xFF};
  check_typed_round_trip(toml_shapes_of(oid(), other));
  check_typed_round_trip(
      toml_shapes_of(dead(0x04), bson::binary{.subtype = std::byte{0x80}, .data = {}}));
  check_typed_round_trip(toml_shapes_of(re(), bson::regex{}));
  check_typed_round_trip(toml_shapes_of(cws(), cws()));
}

TEST_CASE("reflex::serde::bson: TOML reads every bson type as the root document")
{
  const auto sample = toml_typed_sample();
  check_typed_round_trip(dead(0x00));
  check_typed_round_trip(sample.bin);
  check_typed_round_trip(dead(0x80));
  check_typed_round_trip(sample.oid);
  check_typed_round_trip(sample.ts);
  check_typed_round_trip(sample.re);
  check_typed_round_trip(sample.js);
  check_typed_round_trip(sample.cws);
  check_typed_round_trip(sample.sym);
  check_typed_round_trip(sample.dbp);
  check_typed_round_trip(sample.min);
  check_typed_round_trip(sample.max);
  check_typed_round_trip(sample.u);
  const auto roots = written_toml_root();
  CHECK(read_typed_toml<bson::object_id>(roots.at("oid")) == oid());
  CHECK(read_typed_toml<bson::code_w_scope>(roots.at("cws")) == cws());
  CHECK(read_typed_toml<bson::db_pointer>(roots.at("dbp")) == sample.dbp);
}

TEST_CASE("reflex::serde::bson: TOML typed reads name the expected wrapper or the malformed key")
{
  check_typed_throws<bson::object_id>(
      "a = 1", "Extended JSON: expected a $oid document for bson::object_id");
  check_typed_throws<bson::javascript>(
      written_toml_root().at("cws"),
      "Extended JSON: expected a $code document for bson::javascript");
  check_typed_throws<toml_typed_one<bson::code_w_scope>>(
      "v = { \"$code\" = \"g()\" }\nn = 1",
      under("v", "Extended JSON: expected a $code and $scope document for bson::code_w_scope"));
  check_typed_throws<toml_typed_one<bson::timestamp>>(
      "v = { \"$timestamp\" = { t = -1, i = 1 } }\nn = 1",
      under("v", "Extended JSON: $timestamp.t must be an integer from 0 to 4294967295"));
  check_typed_throws<toml_typed_one<bson::max_key>>(
      "v = { \"$maxKey\" = 2 }\nn = 1", under("v", "Extended JSON: $maxKey must be 1"));
}

TEST_CASE("reflex::serde::bson: TOML does not read a typed member from a header table")
{
  check_typed_throws<toml_typed_one<bson::object_id>>(
      std::format("n = 1\n[v]\n\"$oid\" = \"{}\"", oid_hex),
      "TOML: key 'v.$oid': Key not found in object");
  CHECK(
      read_typed_toml<toml_typed_one<bson::object_id>>(
          std::format("n = 1\nv = {{ \"$oid\" = \"{}\" }}", oid_hex))
          .v
      == oid());
}
