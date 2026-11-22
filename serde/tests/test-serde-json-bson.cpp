#include <doctest/doctest.h>

import reflex.serde.json;
import reflex.serde.bson;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::string_view_literals;

#define JSON(...) #__VA_ARGS__

TEST_CASE("reflex::serde::json and bson: interoperability")
{
  using json::null;

  SUBCASE("null")
  {
    auto value = json::deserializer{std::string{"null"}}.load<json::null_t>();
    CHECK(value == null);

    std::vector<std::byte> bson_out;
    bson::serializer       bson_ser{bson_out};
    bson_ser.dump(value);

    auto bson_value = bson::deserializer{bson_out.begin(), bson_out.end()}.load<json::null_t>();
    CHECK(bson_value == null);
  }

  SUBCASE("datetime")
  {
    auto        now          = bson::datetime::now();
    std::string datetime_str = std::format(R"({{"$date":{{"$numberLong":"{}"}}}})", now);
    auto        datetime     = json::deserializer{datetime_str}.load<bson::datetime>();
    CHECK(datetime == now);

    std::ostringstream json_out;
    json::serializer   json_ser{json_out};
    json_ser.dump(now);
    CHECK(json_out.str() == datetime_str);

    std::vector<std::byte> bson_out;
    bson::serializer       bson_ser{bson_out};
    bson_ser.dump(datetime);

    auto bson_value = bson::deserializer{bson_out.begin(), bson_out.end()}.load<bson::datetime>();
    CHECK(bson_value == datetime);
  }

  SUBCASE("bson::value json serialized")
  {
    bson::value typed = bson::object{
        {"i32",  7                                  },
        {"i64",  bson::int64{1ll << 40}             },
        {"d128", bson::decimal128{42.2}             },
        {"dt",   bson::datetime{1'701'234'567'890ll}},
    };
    std::string      out;
    json::serializer ser{out};
    ser.dump(typed);
    std::println("Serialized BSON value as JSON: {}", out);
  }

  SUBCASE("bson::value binary json serialized")
  {
    bson::value typed = bson::binary{
        .data = {std::byte{0xDE}, std::byte{0xAD}}
    };

    std::string      out;
    json::serializer ser{out};
    ser.dump(typed);
    CHECK(out == R"({"$binary":{"base64":"3q0=","subType":"00"}})");
  }
}

TEST_CASE("reflex::serde::json and bson: read a value tree from JSON")
{
  auto const text = std::string{JSON({"a": 1, "b": -5, "c": [2.5, "x", true, null], "d": {"e": 3000000000}})};

  SUBCASE("bson::value")
  {
    auto v = json::deserializer{text}.load<bson::value>();
    CHECK(v["a"] == 1);
    CHECK(v["b"] == -5);
    CHECK(v["b"] != std::numeric_limits<std::size_t>::max());
    CHECK(v["c"][0] == 2.5);
    CHECK(v["c"][1] == "x");
    CHECK(v["c"][2] == true);
    CHECK(v["d.e"] == 3'000'000'000LL);
  }

  SUBCASE("json::value")
  {
    auto v = json::deserializer{text}.load<json::value>();
    CHECK(v["a"] == 1);
    CHECK(v["b"] == -5);
    CHECK(v["c"][1] == "x");
    CHECK(v["d.e"] == 3'000'000'000LL);
  }
}
