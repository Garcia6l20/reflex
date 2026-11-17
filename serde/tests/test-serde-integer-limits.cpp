#include <doctest/doctest.h>

import reflex.serde.toml;
import reflex.serde.yaml;

import std;

using namespace reflex;
using namespace reflex::serde;

template <typename T> struct il_field
{
  T x{};
};

template <typename T> static T il_yaml(std::string const& text)
{
  return yaml::deserializer{text}.template load<T>();
}

template <typename T> static T il_toml(std::string const& token)
{
  return toml::deserializer{"x = " + token}.template load<il_field<T>>().x;
}

template <typename T> static void il_check_limits()
{
  constexpr auto min = std::numeric_limits<T>::min();
  constexpr auto max = std::numeric_limits<T>::max();
  const auto     lo  = std::format("{}", min);
  const auto     hi  = std::format("{}", max);
  CAPTURE(lo);
  CHECK(il_yaml<T>(lo) == min);
  CHECK(il_yaml<T>(hi) == max);
  CHECK(il_toml<T>(lo) == min);
  CHECK(il_toml<T>(hi) == max);

  const auto below = std::format("{}", static_cast<std::int64_t>(min) - 1);
  const auto above = std::format("{}", static_cast<std::uint64_t>(max) + 1);
  CHECK_THROWS_WITH(il_yaml<T>(below), std::format("YAML: '{}' is not an integer", below).c_str());
  CHECK_THROWS_WITH(il_yaml<T>(above), std::format("YAML: '{}' is not an integer", above).c_str());
  CHECK_THROWS_WITH(il_toml<T>(below), std::format("TOML: '{}' is not an integer", below).c_str());
  CHECK_THROWS_WITH(il_toml<T>(above), std::format("TOML: '{}' is not an integer", above).c_str());
}

TEST_CASE("signed integer fields read their minimum and maximum from YAML and TOML")
{
  il_check_limits<std::int8_t>();
  il_check_limits<std::int16_t>();
  il_check_limits<std::int32_t>();
}

TEST_CASE("int64 fields read their minimum and maximum from YAML and TOML")
{
  constexpr auto min = std::numeric_limits<std::int64_t>::min();
  constexpr auto max = std::numeric_limits<std::int64_t>::max();
  CHECK(il_yaml<std::int64_t>("-9223372036854775808") == min);
  CHECK(il_yaml<std::int64_t>("9223372036854775807") == max);
  CHECK(il_toml<std::int64_t>("-9223372036854775808") == min);
  CHECK(il_toml<std::int64_t>("9223372036854775807") == max);
  CHECK_THROWS_WITH(
      il_yaml<std::int64_t>("-9223372036854775809"),
      "YAML: '-9223372036854775809' is not an integer");
  CHECK_THROWS_WITH(
      il_toml<std::int64_t>("9223372036854775808"),
      "TOML: '9223372036854775808' is not an integer");
}

TEST_CASE("prefixed integers into signed fields")
{
  CHECK(il_yaml<std::int8_t>("0x7f") == 127);
  CHECK(il_yaml<std::int8_t>("0o177") == 127);
  CHECK(il_yaml<std::int8_t>("-0x80") == -128);
  CHECK(il_yaml<std::int8_t>("-0o200") == -128);
  CHECK_THROWS_WITH(il_yaml<std::int8_t>("0x80"), "YAML: '0x80' is not an integer");
  CHECK_THROWS_WITH(il_yaml<std::int8_t>("-0x81"), "YAML: '-0x81' is not an integer");

  CHECK(il_toml<std::int8_t>("0x7f") == 127);
  CHECK(il_toml<std::int8_t>("0o177") == 127);
  CHECK(il_toml<std::int8_t>("0b1111111") == 127);
  CHECK_THROWS_WITH(il_toml<std::int8_t>("0x80"), "TOML: '0x80' is not an integer");
  CHECK(il_toml<std::int64_t>("0x7fffffffffffffff") == std::numeric_limits<std::int64_t>::max());
  CHECK_THROWS_WITH(
      il_toml<std::int64_t>("0x8000000000000000"), "TOML: '0x8000000000000000' is not an integer");
}

TEST_CASE("unsigned fields keep rejecting a sign")
{
  CHECK_THROWS_WITH(
      il_yaml<unsigned>("-0"), "YAML: '-0' is negative and the destination is unsigned");
  CHECK_THROWS_WITH(
      il_toml<unsigned>("-0"), "TOML: '-0' is negative and the destination is unsigned");
  CHECK(
      il_yaml<std::uint64_t>("18446744073709551615") == std::numeric_limits<std::uint64_t>::max());
  CHECK_THROWS_WITH(il_toml<std::uint8_t>("256"), "TOML: '256' is not an integer");
}

TEST_CASE("a doubled sign is not an integer")
{
  CHECK_THROWS_WITH(il_yaml<int>("--5"), "YAML: '--5' is not an integer");
  CHECK_THROWS_WITH(il_yaml<int>("+-5"), "YAML: '+-5' is not an integer");
  CHECK_THROWS_WITH(il_yaml<int>("-+5"), "YAML: '-+5' is not an integer");
  CHECK_THROWS_WITH(il_yaml<int>("0x-1"), "YAML: '0x-1' is not an integer");
}
