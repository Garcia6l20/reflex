#include <doctest/doctest.h>

import reflex.serde.json;
import reflex.serde.yaml;
import reflex.serde.bson;

import std;

using namespace reflex;
using namespace reflex::serde;

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

bson::object_id oid_of(std::string_view hex)
{
  bson::object_id id;
  for(std::size_t i = 0; i < id.bytes.size(); ++i)
  {
    unsigned byte = 0;
    std::from_chars(hex.data() + 2 * i, hex.data() + 2 * i + 2, byte, 16);
    id.bytes[i] = static_cast<std::byte>(byte);
  }
  return id;
}

bson::object_id oid()
{
  return oid_of(oid_hex);
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

std::string yaml_of(bson::value const& value)
{
  std::string      out;
  yaml::serializer ser{out};
  ser.dump(value);
  return out;
}

bson::value read_yaml(std::string text)
{
  return yaml::deserializer{text}.load<bson::value>();
}

void check_round_trip(bson::value const& original)
{
  const auto text = yaml_of(original);
  CAPTURE(text);
  const auto read = read_yaml(text);
  CHECK(read.index() == original.index());
  CHECK(read == original);
  CHECK_EQ(yaml_of(read), text);
}

void check_throws(std::string text, std::string const& message)
{
  CAPTURE(text);
  CHECK_THROWS_WITH_AS(read_yaml(text), message.c_str(), std::runtime_error);
}
}

TEST_CASE("reflex::serde::bson: YAML reads each written type back into bson::value")
{
  for(auto const& [key, value] : all_types())
  {
    CAPTURE(key);
    check_round_trip(value);
    check_round_trip(
        bson::object{
            {"k", value}
    });
    check_round_trip(
        bson::object{
            {"outer", bson::object{{"inner", value}, {"after", 3}}}
    });
    check_round_trip(bson::array{value, value});
    check_round_trip(bson::array{bson::value{bson::array{value, value}}});
  }
  check_round_trip(all_types());
}

TEST_CASE("reflex::serde::bson: YAML reads the pinned writer output to the original values")
{
  auto doc = read_yaml(
      std::format(
          "cws:\n  $code: f(x)\n  $scope:\n    x: 7\n"
          "oid:\n  $oid: {}\n"
          "re:\n  $regularExpression:\n    pattern: ^a\"b\\d\n    options: im\n"
          "ts:\n  $timestamp:\n    t: 1700000000\n    i: 2",
          oid_hex));
  CHECK(
      doc
      == bson::value{
          bson::object{{"cws", cws()}, {"oid", oid()}, {"re", re()}, {"ts", ts()}}
  });
  CHECK(doc["cws"].is<bson::code_w_scope>());
  CHECK(doc["oid"].is<bson::object_id>());
  CHECK(doc["re"].is<bson::regex>());
  CHECK(doc["ts"].is<bson::timestamp>());

  CHECK(read_yaml(std::format("$oid: {}", oid_hex)) == bson::value{oid()});
  CHECK(read_yaml("$timestamp:\n  t: 1700000000\n  i: 2") == bson::value{ts()});
  CHECK(read_yaml("$code: f(x)\n$scope:\n  x: 7") == bson::value{cws()});

  CHECK(
      read_yaml("max:\n  $maxKey: 1\nmin:\n  $minKey: 1\nu:\n  $undefined: true")
      == bson::value{
          bson::object{
                       {"max", bson::max_key{}}, {"min", bson::min_key{}}, {"u", bson::undefined{}}}
  });
  CHECK(
      read_yaml("- $minKey: 1\n- $undefined: true")
      == bson::value{
          bson::array{bson::value{bson::min_key{}}, bson::value{bson::undefined{}}}
  });

  CHECK(
      read_yaml("v:\n  $binary:\n    base64: 3q0=\n    subType: '00'")
      == bson::value{bson::object{{"v", dead(0x00)}}});
  CHECK(
      read_yaml("outer:\n  inner:\n    $binary:\n      base64: 3q0=\n      subType: '04'")
      == bson::value{bson::object{{"outer", bson::object{{"inner", dead(0x04)}}}}});
}

TEST_CASE("reflex::serde::bson: YAML reads type wrappers in flow form")
{
  CHECK(read_yaml(std::format("{{$oid: {}}}", oid_hex)) == bson::value{oid()});
  CHECK(read_yaml(R"({"$code": f(x), "$scope": {x: 7}})") == bson::value{cws()});
  CHECK(read_yaml(R"({"$scope": {x: 7}, "$code": f(x)})") == bson::value{cws()});
  CHECK(
      read_yaml(std::format("k: {{$oid: {}}}", oid_hex))
      == bson::value{bson::object{{"k", oid()}}});
  CHECK(
      read_yaml("[{$minKey: 1}, {$binary: {subType: '04', base64: 3q0=}}]")
      == bson::value{
          bson::array{bson::value{bson::min_key{}}, bson::value{dead(0x04)}}
  });
}

TEST_CASE("reflex::serde::bson: YAML quotes an object_id that would resolve as a number")
{
  for(std::string_view hex : {"000000000000000000000e10", "123456789012345678901234"})
  {
    CAPTURE(hex);
    const bson::value id{oid_of(hex)};
    CHECK_EQ(yaml_of(id), std::format("$oid: '{}'", hex));
    CHECK_EQ(
        yaml_of(
            bson::object{
                {"k", id}
    }),
        std::format("k:\n  $oid: '{}'", hex));
    check_round_trip(id);
    check_round_trip(
        bson::object{
            {"k", id}
    });
    check_throws(
        std::format("$oid: {}", hex), "Extended JSON: $oid must be a string of 24 hex digits");
  }
}

TEST_CASE("reflex::serde::bson: YAML subType must be a string")
{
  CHECK(read_yaml("$binary:\n  base64: 3q0=\n  subType: '04'") == bson::value{dead(0x04)});
  CHECK(read_yaml("$binary:\n  base64: 3q0=\n  subType: '4'") == bson::value{dead(0x04)});
  CHECK(read_yaml("$binary:\n  base64: 3q0=\n  subType: \"8a\"") == bson::value{dead(0x8A)});
  check_throws(
      "$binary:\n  base64: 3q0=\n  subType: 04",
      "Extended JSON: $binary.subType must be a string of 1 or 2 hex digits");
}

TEST_CASE("reflex::serde::bson: YAML reads the canonical wrappers")
{
  const auto l = read_yaml("$numberLong: '5'");
  REQUIRE(l.is<bson::int64>());
  CHECK(l.as<bson::int64>() == 5);

  auto i = read_yaml("x:\n  $numberInt: '-7'");
  REQUIRE(i["x"].is<bson::int32>());
  CHECK(i["x"].as<bson::int32>() == -7);

  const auto d = read_yaml("$numberDouble: '1.5'");
  REQUIRE(d.is<double>());
  CHECK(d.as<double>() == 1.5);

  const auto m = read_yaml("$numberDecimal: '1.5'");
  REQUIRE(m.is<bson::decimal128>());
  CHECK(m.as<bson::decimal128>() == bson::decimal128{1.5});

  const auto date = read_yaml("$date: '2023-11-14T22:13:20.123Z'");
  REQUIRE(date.is<bson::datetime>());
  CHECK(date.as<bson::datetime>().millis_since_epoch == 1700000000123);

  const auto canonical_date = read_yaml("$date:\n  $numberLong: '1700000000123'");
  REQUIRE(canonical_date.is<bson::datetime>());
  CHECK(canonical_date.as<bson::datetime>().millis_since_epoch == 1700000000123);
}

TEST_CASE("reflex::serde::bson: YAML number wrappers take a string body only")
{
  check_throws(
      "$numberInt: 5", "Extended JSON: $numberInt must be a string holding a 32-bit integer");
  check_throws(
      "$numberLong: 5", "Extended JSON: $numberLong must be a string holding a 64-bit integer");
  check_throws(
      "$numberDouble: 1.5", "Extended JSON: $numberDouble must be a string holding a double");
  check_throws(
      "$numberDecimal: 1.5", "Extended JSON: $numberDecimal must be a string holding a decimal128");
  check_throws(
      "x:\n  $numberDecimal: 1.5",
      "Extended JSON: $numberDecimal must be a string holding a decimal128");
}

TEST_CASE("reflex::serde::bson: YAML $numberDouble takes the spec spellings, not .nan or .inf")
{
  const std::string bad_double = "Extended JSON: $numberDouble must be a string holding a double";
  check_throws("$numberDouble: .nan", bad_double);
  check_throws("$numberDouble: .inf", bad_double);
  check_throws("$numberDouble: -.inf", bad_double);
  check_throws("$numberDouble: '.nan'", bad_double);

  for(std::string_view text : {"'NaN'", "NaN", "\"NaN\""})
  {
    CAPTURE(text);
    const auto read = read_yaml(std::format("$numberDouble: {}", text));
    REQUIRE(read.is<double>());
    CHECK(std::isnan(read.as<double>()));
  }
  for(std::string_view text : {"'Infinity'", "Infinity"})
  {
    CAPTURE(text);
    const auto read = read_yaml(std::format("$numberDouble: {}", text));
    REQUIRE(read.is<double>());
    CHECK(read.as<double>() == std::numeric_limits<double>::infinity());
  }
  const auto negative = read_yaml("$numberDouble: '-Infinity'");
  REQUIRE(negative.is<double>());
  CHECK(negative.as<double>() == -std::numeric_limits<double>::infinity());

  auto native = read_yaml("x: .nan");
  REQUIRE(native["x"].is<double>());
  CHECK(std::isnan(native["x"].as<double>()));
}

TEST_CASE("reflex::serde::bson: YAML leaves mappings without a reserved key as mappings")
{
  const std::array<std::string_view, 4> texts{
      "$set:\n  a: 1",
      "$type: 2",
      "$id: 1\n$ref: c",
      "$db: d\n$id: 1\n$ref: c",
  };
  for(auto text : texts)
  {
    CAPTURE(text);
    const auto read = read_yaml(std::string{text});
    REQUIRE(read.is_object());
    CHECK_EQ(yaml_of(read), text);
  }
  CHECK(read_yaml("$set:\n  a: 1")["$set"]["a"] == 1);

  auto dbref = read_yaml(std::format("$ref: c\n$id:\n  $oid: {}", oid_hex));
  REQUIRE(dbref.is_object());
  CHECK(dbref["$id"] == bson::value{oid()});
  CHECK(dbref["$ref"] == "c");
}

TEST_CASE("reflex::serde::bson: yaml::value keeps $oid as a plain mapping")
{
  const auto read = yaml::deserializer{std::string{"$oid: x"}}.load<yaml::value>();
  REQUIRE(read.is_object());
  CHECK(read.as_object().at("$oid") == "x");
}

TEST_CASE("reflex::serde::bson: YAML throws on a malformed type wrapper, naming the key")
{
  check_throws(
      std::format("$oid: {}\nx: 1", oid_hex),
      "Extended JSON: unexpected key 'x' in a $oid document");
  check_throws(
      "$binary:\n  base64: 3q0=", "Extended JSON: a $binary document is missing 'subType'");
  check_throws("$scope:\n  x: 7", "Extended JSON: a $code document is missing '$code'");
  check_throws("$oid: zz", "Extended JSON: $oid must be a string of 24 hex digits");
  check_throws(
      "- k:\n    $oid: 5061728394a5b6c7d8e9fa0",
      "Extended JSON: $oid must be a string of 24 hex digits");
  check_throws("$undefined: 'true'", "Extended JSON: $undefined must be true");
  check_throws("$minKey: '1'", "Extended JSON: $minKey must be 1");
}

namespace
{
struct read_yaml_commented
{
  bson::value v;
  int         n = 0;
};

constexpr yaml::read_options read_yaml_keep{.comments = true};

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
    "Null": null,
    "Undefined": {"$undefined": true}
  })";
}
}

TEST_CASE("reflex::serde::bson: YAML comments mode lifts a wrapper and keeps its comment")
{
  const auto         text = std::format("v: # tv\n  $oid: {}\nn: 3\n", oid_hex);
  yaml::deserializer de{text, std::cw<read_yaml_keep>};
  const auto [value, comments] = de.load_commented<read_yaml_commented>();
  REQUIRE(value.v.is<bson::object_id>());
  CHECK(value.v == bson::value{oid()});
  CHECK(value.n == 3);
  CHECK_EQ(comments.v.trailing, "tv");
}

TEST_CASE("reflex::serde::bson: YAML reads the canonical document as JSON does")
{
  const auto  json_text = canonical_json();
  std::string yaml_text;
  yaml::serializer{yaml_text}.dump(json::deserializer{json_text}.load<json::value>());
  CAPTURE(yaml_text);

  const auto from_json = json::deserializer{json_text}.load<bson::value>();
  const auto from_yaml = read_yaml(yaml_text);
  REQUIRE(from_json.is_object());
  REQUIRE(from_yaml.is_object());
  auto const& expected = from_json.as_object();
  auto const& actual   = from_yaml.as_object();
  CHECK(actual.size() == 28);
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
    else if(key == "Null")
    {
      CHECK(actual.at(key).is_null());
    }
    else
    {
      CHECK(actual.at(key) == value);
    }
  }
}

namespace
{
struct yaml_typed
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

  bool operator==(yaml_typed const&) const = default;
};

template <typename T> struct yaml_typed_one
{
  T   v;
  int n = 0;
};

template <typename T> struct yaml_shapes
{
  std::optional<T>         set;
  std::optional<T>         none;
  std::vector<T>           list;
  std::map<std::string, T> map;
  int                      tail = 5;

  bool operator==(yaml_shapes const&) const = default;
};

template <typename T> yaml_shapes<T> yaml_shapes_of(T const& first, T const& second)
{
  return {
      .set  = first,
      .none = std::nullopt,
      .list = {first,         second        },
      .map  = {{"k1", first}, {"k2", second}},
  };
}

yaml_typed yaml_typed_sample()
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

template <typename T> std::string typed_yaml_of(T const& value)
{
  std::string      out;
  yaml::serializer ser{out};
  ser.dump(value);
  return out;
}

template <typename T> T read_typed_yaml(std::string text)
{
  return yaml::deserializer{text}.load<T>();
}

template <typename T> void check_typed_round_trip(T const& original)
{
  const auto text = typed_yaml_of(original);
  CAPTURE(text);
  const auto read = read_typed_yaml<T>(text);
  CHECK(read == original);
  CHECK_EQ(typed_yaml_of(read), text);
}

template <typename T> void check_typed_throws(std::string text, std::string const& message)
{
  CAPTURE(text);
  CHECK_THROWS_WITH_AS(read_typed_yaml<T>(text), message.c_str(), std::runtime_error);
}
}

TEST_CASE("reflex::serde::bson: YAML reads every bson type as a typed member")
{
  const auto sample = yaml_typed_sample();
  check_typed_round_trip(sample);
  CHECK(
      read_typed_yaml<yaml_typed_one<bson::object_id>>(
          std::format("v: {{$oid: '{}'}}\nn: 1", oid_hex))
          .v
      == oid());
  CHECK(
      read_typed_yaml<yaml_typed_one<bson::code_w_scope>>(
          "v:\n  $scope:\n    x: 7\n  $code: f(x)\nn: 1")
          .v
      == cws());
}

TEST_CASE("reflex::serde::bson: YAML reads bson types in optional, vector and map members")
{
  auto other     = oid();
  other.bytes[0] = std::byte{0xFF};
  check_typed_round_trip(yaml_shapes_of(oid(), other));
  check_typed_round_trip(
      yaml_shapes_of(dead(0x04), bson::binary{.subtype = std::byte{0x80}, .data = {}}));
  check_typed_round_trip(yaml_shapes_of(re(), bson::regex{}));
  check_typed_round_trip(yaml_shapes_of(cws(), cws()));
  CHECK(
      read_typed_yaml<std::vector<bson::object_id>>(
          std::format("- $oid: '{0}'\n- {{$oid: '{0}'}}", oid_hex))
      == std::vector{oid(), oid()});
}

TEST_CASE("reflex::serde::bson: YAML reads every bson type as the root")
{
  const auto sample = yaml_typed_sample();
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
  CHECK(read_typed_yaml<bson::object_id>(std::format("$oid: '{}'", oid_hex)) == oid());
}

TEST_CASE("reflex::serde::bson: YAML typed reads name the expected wrapper or the malformed key")
{
  check_typed_throws<bson::object_id>(
      "a: 1", "Extended JSON: expected a $oid document for bson::object_id");
  check_typed_throws<bson::javascript>(
      "$code: f(x)\n$scope:\n  x: 7",
      "Extended JSON: expected a $code document for bson::javascript");
  check_typed_throws<bson::code_w_scope>(
      "$code: g()", "Extended JSON: expected a $code and $scope document for bson::code_w_scope");
  check_typed_throws<yaml_typed_one<bson::object_id>>(
      "v:\n  $oid: zz\nn: 1", "Extended JSON: $oid must be a string of 24 hex digits");
  check_typed_throws<yaml_typed_one<bson::binary>>(
      "v:\n  $binary:\n    base64: 3q0=\nn: 1",
      "Extended JSON: a $binary document is missing 'subType'");
  check_typed_throws<yaml_typed_one<bson::undefined>>(
      "v:\n  $undefined: false\nn: 1", "Extended JSON: $undefined must be true");
}

namespace
{
bson::object every_alternative()
{
  auto all    = all_types();
  all["i32"]  = bson::int32{-7};
  all["i64"]  = bson::int64{5000000000};
  all["num"]  = 2.5;
  all["bool"] = true;
  all["str"]  = std::string{"s"};
  all["d128"] = bson::decimal128{1.5};
  all["dt"]   = bson::datetime{1700000000123};
  return all;
}
}

TEST_CASE("reflex::serde::bson: YAML reads and writes a bson::value holding every alternative")
{
  const auto all = every_alternative();
  REQUIRE(all.size() == 20);
  check_round_trip(all);
  const auto read = read_yaml(yaml_of(all));
  for(auto const& [key, value] : all)
  {
    CAPTURE(key);
    CHECK(read.as_object().at(key).index() == value.index());
  }
}

TEST_CASE("reflex::serde::bson: YAML writes bson::datetime as a canonical $date document")
{
  const bson::datetime date{1700000000123};
  CHECK_EQ(typed_yaml_of(date), "$date:\n  $numberLong: '1700000000123'");
  CHECK_EQ(typed_yaml_of(bson::datetime{-1}), "$date:\n  $numberLong: '-1'");
  CHECK_EQ(
      typed_yaml_of(yaml_typed_one<bson::datetime>{.v = date, .n = 1}),
      "v:\n  $date:\n    $numberLong: '1700000000123'\n'n': 1");
  CHECK_EQ(
      yaml_of(
          bson::object{
              {"d", date                                 },
              {"a", bson::array{date, bson::datetime{-1}}},
  }),
      "a:\n  - $date:\n      $numberLong: '1700000000123'\n  - $date:\n      $numberLong: '-1'\n"
      "d:\n  $date:\n    $numberLong: '1700000000123'");
}

TEST_CASE("reflex::serde::bson: YAML reads bson::datetime in both $date forms")
{
  const bson::datetime date{1700000000123};
  check_round_trip(date);
  check_round_trip(bson::datetime{-1});
  check_round_trip(
      bson::object{
          {"d", date},
  });
  check_round_trip(bson::array{bson::value{bson::array{date, bson::datetime{-1}}}});
  check_typed_round_trip(date);
  check_typed_round_trip(bson::datetime{-1});
  check_typed_round_trip(yaml_shapes_of(date, bson::datetime{-1}));

  CHECK(read_typed_yaml<bson::datetime>("$date: 2023-11-14T22:13:20.123Z") == date);
  CHECK(
      read_typed_yaml<yaml_typed_one<bson::datetime>>(
          "v:\n  $date: '2023-11-14T22:13:20.123Z'\nn: 1")
          .v
      == date);
  CHECK(
      read_typed_yaml<yaml_typed_one<bson::datetime>>(
          "v: {$date: {$numberLong: '1700000000123'}}\nn: 1")
          .v
      == date);
}

TEST_CASE("reflex::serde::bson: YAML typed bson::datetime reads throw on another document")
{
  check_typed_throws<bson::datetime>(
      "a: 1", "Extended JSON: expected a $date document for bson::datetime");
  check_typed_throws<yaml_typed_one<bson::datetime>>(
      "v:\n  $date: true\nn: 1",
      "Extended JSON: $date must be an RFC 3339 string or a $numberLong document");
}
