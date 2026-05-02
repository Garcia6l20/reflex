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
  std::istringstream in;

  using json::null;

  SUBCASE("null")
  {
    in.str("null");
    auto value = json::deserializer::load<json::null_t>(in);
    CHECK(value == null);

    std::ostringstream bson_out;
    bson::serializer       bson_serializer;
    bson_serializer(bson_out, value);

    auto bson_value = bson::deserializer::load<json::null_t>(bson_out.str());
    CHECK(bson_value == null);
  }

  SUBCASE("datetime")
  {
    auto now = bson::datetime::now();
    std::string datetime_str = std::format("\"{}\"", now);
    auto        datetime     = json::deserializer::load<bson::datetime>(datetime_str);
    std::println("Parsed datetime: {}", datetime);
    CHECK(datetime == now);

    json::serializer json_serializer;
    std::ostringstream json_out;
    json_serializer(json_out, now);
    CHECK(json_out.str() == datetime_str);

    std::ostringstream bson_out;
    bson::serializer       bson_serializer;
    bson_serializer(bson_out, datetime);

    auto bson_value = bson::deserializer::load<bson::datetime>(bson_out.str());
    CHECK(bson_value == datetime);
  }
}
