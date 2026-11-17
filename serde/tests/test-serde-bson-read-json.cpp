#include <doctest/doctest.h>

import reflex.serde.json;
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

namespace
{
using bytes = std::vector<std::byte>;

constexpr std::string_view oid_hex = "5061728394a5b6c7d8e9fa0b";

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

bson::object_id oid()
{
  bson::object_id id;
  for(std::size_t i = 0; i < id.bytes.size(); ++i)
  {
    id.bytes[i] = static_cast<std::byte>(0x50 + i * 17);
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

bson::object all_types()
{
  return {
      {"bin",  dead(0x00)                                            },
      {"bin4", dead(0x04)                                            },
      {"bin8", dead(0x80)                                            },
      {"oid",  oid()                                                 },
      {"ts",   bson::timestamp{.increment = 2, .seconds = 1700000000}},
      {"re",   bson::regex{.pattern = R"(^a"b\d)", .options = "im"}  },
      {"js",   bson::javascript{"g()"}                               },
      {"cws",  cws()                                                 },
      {"sym",  bson::symbol{"s"}                                     },
      {"dbp",  bson::db_pointer{.ref = "db.coll", .id = oid()}       },
      {"min",  bson::min_key{}                                       },
      {"max",  bson::max_key{}                                       },
      {"u",    bson::undefined{}                                     },
  };
}

std::map<std::string, std::string> written_json()
{
  const auto oid_json = std::format(R"({{"$oid":"{}"}})", oid_hex);
  return {
      {"bin", R"j({"$binary":{"base64":"3q0=","subType":"00"}})j"},
      {"bin4", R"j({"$binary":{"base64":"3q0=","subType":"04"}})j"},
      {"bin8", R"j({"$binary":{"base64":"3q0=","subType":"80"}})j"},
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

std::string json_of(bson::value const& value)
{
  std::string      out;
  json::serializer ser{out};
  ser.dump(value);
  return out;
}

bson::value read_json(std::string text)
{
  return json::deserializer{text}.load<bson::value>();
}

void check_round_trip(bson::value const& original)
{
  const auto text = json_of(original);
  CAPTURE(text);
  const auto read = read_json(text);
  CHECK(read.index() == original.index());
  CHECK(read == original);
  CHECK_EQ(json_of(read), text);
}

void check_throws(std::string text, std::string const& message)
{
  CAPTURE(text);
  CHECK_THROWS_WITH_AS(read_json(text), message.c_str(), std::runtime_error);
}
}

TEST_CASE("reflex::serde::bson: JSON writes every type inside bson::value with no envelope")
{
  const auto  forms = written_json();
  std::string members;
  for(auto const& [key, value] : all_types())
  {
    members += std::format("{}\"{}\":{}", members.empty() ? "" : ",", key, forms.at(key));
  }
  CHECK_EQ(json_of(bson::value{all_types()}), std::format("{{{}}}", members));
}

TEST_CASE("reflex::serde::bson: JSON reads each written type back into bson::value")
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
    check_round_trip(bson::array{bson::value{bson::array{value, value}}});
  }
  check_round_trip(all_types());
}

TEST_CASE("reflex::serde::bson: JSON reads the pinned writer output to the original values")
{
  const auto values = all_types();
  for(auto const& [key, text] : written_json())
  {
    CAPTURE(key);
    const auto read = read_json(text);
    CHECK(read.index() == values.at(key).index());
    CHECK(read == values.at(key));
  }
  CHECK(read_json(written_json().at("oid")).is<bson::object_id>());
  CHECK(read_json(written_json().at("cws")).is<bson::code_w_scope>());
  CHECK(read_json(written_json().at("js")).is<bson::javascript>());
}

TEST_CASE("reflex::serde::bson: JSON reads $scope into the scope bytes the BSON writer produces")
{
  const bson::object scope{
      {"x", 7    },
      {"o", oid()},
  };
  bytes            encoded;
  bson::serializer ser{encoded};
  serde::serialize(ser, scope);

  const auto read =
      read_json(std::format(R"({{"$code":"f","$scope":{{"x":7,"o":{{"$oid":"{}"}}}}}})", oid_hex));
  REQUIRE(read.is<bson::code_w_scope>());
  CHECK(read.as<bson::code_w_scope>().scope == encoded);
  CHECK(
      read
      == bson::value{
          bson::code_w_scope{.code = "f", .scope = encoded}
  });

  const auto back = bson::deserializer{encoded.begin(), encoded.end()}.load<bson::value>();
  CHECK(back.as_object().at("x").is<bson::int32>());
  CHECK(back.as_object().at("o").is<bson::object_id>());
}

TEST_CASE("reflex::serde::bson: JSON key order inside a type wrapper is not significant")
{
  CHECK(read_json(R"j({"$scope":{"x":7},"$code":"f(x)"})j") == bson::value{cws()});
  CHECK(read_json(R"j({"$code":"f(x)","$scope":{"x":7}})j") == bson::value{cws()});
  CHECK(read_json(R"({"$binary":{"subType":"04","base64":"3q0="}})") == bson::value{dead(0x04)});
  CHECK(
      read_json(R"({"$timestamp":{"i":2,"t":1700000000}})")
      == bson::value{
          bson::timestamp{.increment = 2, .seconds = 1700000000}
  });
  CHECK(
      read_json(std::format(R"({{"$dbPointer":{{"$id":{{"$oid":"{}"}},"$ref":"c"}}}})", oid_hex))
      == bson::value{
          bson::db_pointer{.ref = "c", .id = oid()}
  });
}

TEST_CASE("reflex::serde::bson: JSON $binary decodes the RFC 4648 base64 vectors")
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
  for(auto const& [plain, encoded] : vectors)
  {
    CAPTURE(plain);
    const auto read =
        read_json(std::format(R"({{"$binary":{{"base64":"{}","subType":"00"}}}})", encoded));
    CHECK(read == bson::value{bson::binary{.data = text_bytes(plain)}});
  }
  CHECK(
      read_json(R"({"$binary":{"base64":"+/+/","subType":"ff"}})")
      == bson::value{
          bson::binary{
                       .subtype = std::byte{0xFF},
                       .data    = {std::byte{0xFB}, std::byte{0xFF}, std::byte{0xBF}}}
  });
}

TEST_CASE("reflex::serde::bson: JSON reads hex digits in either case")
{
  CHECK(read_json(R"({"$oid":"5061728394A5B6C7D8E9FA0B"})") == bson::value{oid()});
  CHECK(read_json(R"({"$oid":"5061728394a5B6c7D8e9Fa0b"})") == bson::value{oid()});
  CHECK(read_json(R"({"$binary":{"base64":"3q0=","subType":"4"}})") == bson::value{dead(0x04)});
  CHECK(read_json(R"({"$binary":{"base64":"3q0=","subType":"04"}})") == bson::value{dead(0x04)});
  CHECK(read_json(R"({"$binary":{"base64":"3q0=","subType":"8A"}})") == bson::value{dead(0x8A)});
  CHECK(read_json(R"({"$binary":{"base64":"3q0=","subType":"8a"}})") == bson::value{dead(0x8A)});
}

TEST_CASE("reflex::serde::bson: JSON leaves objects without a reserved key as objects")
{
  const std::array<std::string_view, 7> texts{
      R"({"$set":{"a":1}})",
      R"({"$type":2})",
      R"({"$options":"i","$regex":"a"})",
      R"({"$uuid":"00112233-4455-6677-8899-aabbccddeeff"})",
      R"({"$id":1,"$ref":"c"})",
      R"({"$db":"d","$id":1,"$ref":"c"})",
      R"({})",
  };
  for(auto text : texts)
  {
    CAPTURE(text);
    const auto read = read_json(std::string{text});
    REQUIRE(read.is_object());
    CHECK_EQ(json_of(read), text);
  }
  CHECK(read_json(R"({"$set":{"a":1}})").as_object().at("$set").as_object().at("a") == 1);

  const auto dbref = read_json(R"({"$ref":"c","$id":{"$oid":"5061728394a5b6c7d8e9fa0b"}})");
  REQUIRE(dbref.is_object());
  CHECK(dbref.as_object().at("$id") == bson::value{oid()});
  CHECK(dbref.as_object().at("$ref") == "c");
}

TEST_CASE("reflex::serde::bson: json::value keeps $oid as a plain object")
{
  const auto read =
      json::deserializer{std::string{R"({"$oid":"5061728394a5b6c7d8e9fa0b"})"}}.load<json::value>();
  REQUIRE(read.is_object());
  CHECK(read.as_object().at("$oid") == "5061728394a5b6c7d8e9fa0b");
}

TEST_CASE("reflex::serde::bson: JSON throws on a malformed type wrapper, naming the key")
{
  const auto oid_json = std::format(R"("{}")", oid_hex);

  check_throws(R"({"$oid":"zz"})", "Extended JSON: $oid must be a string of 24 hex digits");
  check_throws(R"({"$oid":1})", "Extended JSON: $oid must be a string of 24 hex digits");
  check_throws(
      R"({"$oid":"5061728394a5b6c7d8e9fa0"})",
      "Extended JSON: $oid must be a string of 24 hex digits");
  check_throws(
      std::format(R"({{"$oid":{},"x":1}})", oid_json),
      "Extended JSON: unexpected key 'x' in a $oid document");
  check_throws(
      std::format(R"({{"$oid":{},"$symbol":"s"}})", oid_json),
      "Extended JSON: unexpected key '$symbol' in a $oid document");
  check_throws(R"({"$scope":{}})", "Extended JSON: a $code document is missing '$code'");
  check_throws(R"({"$code":"f","x":1})", "Extended JSON: unexpected key 'x' in a $code document");
  check_throws(R"({"$code":1})", "Extended JSON: $code must be a string");
  check_throws(R"({"$code":"f","$scope":1})", "Extended JSON: $scope must be a document");
  check_throws(R"({"$binary":"3q0="})", "Extended JSON: $binary must be a document");
  check_throws(
      R"({"$binary":{"base64":"3q0="}})", "Extended JSON: a $binary document is missing 'subType'");
  check_throws(
      R"({"$binary":{"base64":"3q0=","subType":"00","x":1}})",
      "Extended JSON: unexpected key 'x' in a $binary document");
  check_throws(
      R"({"$binary":{"base64":"3q0","subType":"00"}})",
      "Extended JSON: $binary.base64 must be a padded base64 string");
  check_throws(
      R"({"$binary":{"base64":"3q=0","subType":"00"}})",
      "Extended JSON: $binary.base64 must be a padded base64 string");
  check_throws(
      R"({"$binary":{"base64":1,"subType":"00"}})",
      "Extended JSON: $binary.base64 must be a padded base64 string");
  check_throws(
      R"({"$binary":{"base64":"3q0=","subType":"100"}})",
      "Extended JSON: $binary.subType must be a string of 1 or 2 hex digits");
  check_throws(
      R"({"$binary":{"base64":"3q0=","subType":""}})",
      "Extended JSON: $binary.subType must be a string of 1 or 2 hex digits");
  check_throws(
      R"({"$binary":{"base64":"3q0=","subType":"0g"}})",
      "Extended JSON: $binary.subType must be a string of 1 or 2 hex digits");
  check_throws(
      R"({"$binary":{"base64":"3q0=","subType":4}})",
      "Extended JSON: $binary.subType must be a string of 1 or 2 hex digits");
  check_throws(
      R"({"$timestamp":{"t":-1,"i":0}})",
      "Extended JSON: $timestamp.t must be an integer from 0 to 4294967295");
  check_throws(
      R"({"$timestamp":{"t":4294967296,"i":0}})",
      "Extended JSON: $timestamp.t must be an integer from 0 to 4294967295");
  check_throws(
      R"({"$timestamp":{"t":1.0,"i":0}})",
      "Extended JSON: $timestamp.t must be an integer from 0 to 4294967295");
  check_throws(
      R"({"$timestamp":{"t":0,"i":"0"}})",
      "Extended JSON: $timestamp.i must be an integer from 0 to 4294967295");
  check_throws(R"({"$timestamp":{"t":0}})", "Extended JSON: a $timestamp document is missing 'i'");
  check_throws(
      R"({"$regularExpression":{"pattern":1,"options":""}})",
      "Extended JSON: $regularExpression.pattern must be a string");
  check_throws(
      R"({"$regularExpression":{"pattern":"a","options":null}})",
      "Extended JSON: $regularExpression.options must be a string");
  check_throws(R"({"$symbol":1})", "Extended JSON: $symbol must be a string");
  check_throws(
      R"({"$dbPointer":{"$ref":"c","$id":"x"}})",
      "Extended JSON: $dbPointer.$id must be an $oid document");
  check_throws(
      std::format(R"({{"$dbPointer":{{"$ref":1,"$id":{{"$oid":{}}}}}}})", oid_json),
      "Extended JSON: $dbPointer.$ref must be a string");
  check_throws(
      R"({"$dbPointer":{"$ref":"c"}})", "Extended JSON: a $dbPointer document is missing '$id'");
  check_throws(R"({"$minKey":2})", "Extended JSON: $minKey must be 1");
  check_throws(R"({"$maxKey":1.0})", "Extended JSON: $maxKey must be 1");
  check_throws(R"({"$undefined":false})", "Extended JSON: $undefined must be true");
  check_throws(R"({"$undefined":1})", "Extended JSON: $undefined must be true");
  check_throws(
      R"({"a":[{"b":{"$oid":"zz"}}]})", "Extended JSON: $oid must be a string of 24 hex digits");
}

TEST_CASE("reflex::serde::bson: JSON $binary rejects non-zero base64 pad bits")
{
  check_throws(
      R"({"$binary":{"base64":"3q1=","subType":"00"}})",
      "Extended JSON: $binary.base64 must be a padded base64 string");
  check_throws(
      R"({"$binary":{"base64":"Zh==","subType":"00"}})",
      "Extended JSON: $binary.base64 must be a padded base64 string");
}

TEST_CASE("reflex::serde::bson: JSON $scope the BSON writer refuses throws with the prefix")
{
  check_throws(
      R"({"$code":"f","$scope":{"r":{"$regularExpression":{"pattern":"a","options":"q"}}}})",
      "Extended JSON: $scope is not a valid BSON document: "
      "BSON regex options may only hold the letters i, l, m, s, u, x");
  check_throws(
      R"({"$code":"f","$scope":{"a\u0000b":1}})",
      "Extended JSON: $scope is not a valid BSON document: BSON key must not contain a null byte");
  check_throws(
      R"({"$code":"f","$scope":{"o":{"a\u0000b":1}}})",
      "Extended JSON: $scope is not a valid BSON document: BSON key must not contain a null byte");
}

TEST_CASE("reflex::serde::bson: the BSON writer refuses a key holding a null byte")
{
  const auto key = std::string{"a\0b", 3};
  bytes      out;

  bson::serializer flat{out};
  CHECK_THROWS_WITH_AS(
      serde::serialize(
          flat,
          bson::object{
              {key, 1}
  }),
      "BSON key must not contain a null byte", std::invalid_argument);

  bson::serializer nested{out};
  CHECK_THROWS_WITH_AS(
      serde::serialize(
          nested,
          bson::object{
              {"o", bson::object{{key, 1}}}
  }),
      "BSON key must not contain a null byte", std::invalid_argument);

  bson::serializer in_map{out};
  CHECK_THROWS_WITH_AS(
      serde::serialize(
          in_map,
          std::map<std::string, int>{
              {key, 1}
  }),
      "BSON key must not contain a null byte", std::invalid_argument);
}

TEST_CASE("reflex::serde::bson: JSON round trips that do not give back the original value")
{
  const bson::regex unsorted{.pattern = "a", .options = "xi"};
  const auto        text = json_of(unsorted);
  CHECK_EQ(text, R"({"$regularExpression":{"pattern":"a","options":"ix"}})");
  CHECK(
      read_json(text)
      == bson::value{
          bson::regex{.pattern = "a", .options = "ix"}
  });
  CHECK(read_json(text) != bson::value{unsorted});
  CHECK(
      read_json(R"({"$regularExpression":{"pattern":"a","options":"xi"}})")
      == bson::value{unsorted});

  const bson::object widened{
      {"d", 1.0           },
      {"i", 3             },
      {"l", bson::int64{7}},
  };
  const bson::object narrowed{
      {"d", 1},
      {"i", 3},
      {"l", 7},
  };
  bytes            widened_bytes;
  bson::serializer widened_ser{widened_bytes};
  serde::serialize(widened_ser, widened);
  bytes            narrowed_bytes;
  bson::serializer narrowed_ser{narrowed_bytes};
  serde::serialize(narrowed_ser, narrowed);

  const bson::code_w_scope original{.code = "f", .scope = widened_bytes};
  const auto               written = json_of(original);
  CHECK_EQ(written, R"({"$code":"f","$scope":{"d":1,"i":3,"l":7}})");
  const auto read = read_json(written);
  REQUIRE(read.is<bson::code_w_scope>());
  CHECK(read.as<bson::code_w_scope>().scope == narrowed_bytes);
  CHECK(read != bson::value{original});

  const auto scope =
      bson::deserializer{narrowed_bytes.begin(), narrowed_bytes.end()}.load<bson::value>();
  CHECK(scope.as_object().at("d").is<bson::int32>());
  CHECK(scope.as_object().at("l").is<bson::int32>());
}
