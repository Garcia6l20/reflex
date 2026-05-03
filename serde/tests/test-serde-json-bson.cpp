#include <doctest/doctest.h>

import reflex.serde.json;
import reflex.serde.bson;

import std;

using namespace reflex;
using namespace reflex::serde;
using namespace std::string_view_literals;

#define JSON(...) #__VA_ARGS__

namespace reflex::serde::json
{
template <typename It>
OutputIt tag_invoke(tag_t<serde::serialize>, serializer<It>& ser, bson::datetime const& dt)
{
  return tag_invoke(tag_t<serde::serialize>{}, ser, std::format("{}", dt));
}

template <typename It>
bson::datetime
    tag_invoke(tag_t<serde::deserialize>, deserializer<It>& de, std::type_identity<bson::datetime>)
{
  auto s = tag_invoke(tag_t<serde::deserialize>{}, de, std::type_identity<std::string>{});
  return reflex::parse_or_throw<bson::datetime>(s);
}
} // namespace reflex::serde::json

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
    std::string datetime_str = std::format("\"{}\"", now);
    auto        datetime     = json::deserializer{datetime_str}.load<bson::datetime>();
    std::println("Parsed datetime: {}", datetime);
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
}
