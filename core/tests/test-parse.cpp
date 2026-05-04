#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;
using namespace std::literals;

TEST_CASE("reflex::parse: base types")
{
  SUBCASE("int")
  {
    constexpr auto input  = "42";
    constexpr auto result = parse<int>(input);
    REQUIRE(result.has_value());
    CHECK(result.value() == 42);
    CHECK(result.end() == input + 2);
  }
  SUBCASE("int - negative")
  {
    constexpr auto input  = "-42";
    constexpr auto result = parse<int>(input);
    REQUIRE(result.has_value());
    CHECK(result.value() == -42);
    CHECK(result.end() == input + 3);
  }
  SUBCASE("int - hex")
  {
    constexpr auto input  = "0x2a";
    constexpr auto result = parse<int>(input);
    REQUIRE(result.has_value());
    CHECK(result.value() == 42);
    CHECK(result.end() == input + 4);
  }
  SUBCASE("int - binary")
  {
    constexpr auto input  = "0b101010";
    constexpr auto result = parse<int>(input);
    REQUIRE(result.has_value());
    CHECK(result.value() == 42);
    CHECK(result.end() == input + 8);
  }
  SUBCASE("double")
  {
    constexpr auto input  = "3.14";
    const auto     result = parse<double>(input);
    REQUIRE(result.has_value());
    CHECK(result.value() == doctest::Approx(3.14));
    CHECK(result.end() == input + 4);
  }
  SUBCASE("string")
  {
    constexpr auto input  = "hello";
    const auto     result = parse<std::string>(input);
    REQUIRE(result.has_value());
    CHECK(result.value() == "hello");
    CHECK(result.end() == input + 5);
  }
  SUBCASE("bool - true")
  {
    constexpr auto input  = "true";
    const auto     result = parse<bool>(input);
    REQUIRE(result.has_value());
    CHECK(result.value() == true);
    CHECK(result.end() == input + 4);
  }
  SUBCASE("bool - false")
  {
    constexpr auto input  = "false";
    const auto     result = parse<bool>(input);
    REQUIRE(result.has_value());
    CHECK(result.value() == false);
    CHECK(result.end() == input + 5);
  }
  SUBCASE("chrono::system_clock::time_point")
  {
    constexpr auto input = "2026-01-02T12:30:42.250";
    const auto result    = parse<std::chrono::system_clock::time_point, "%Y-%m-%dT%H:%M:%S">(input);
    REQUIRE(result.has_value());
    const auto tp = std::chrono::system_clock::time_point{std::chrono::milliseconds{1767357042250}};
    CHECK(result.value() == tp);
    CHECK(result.end() == input + 23);
  }
}

TEST_CASE("reflex::parse: failures return error codes")
{
  auto invalid_int = parse<int>("42x");
  REQUIRE_FALSE(invalid_int.has_value());
  CHECK_EQ(invalid_int.error(), std::errc::invalid_argument);

  auto invalid_bool = parse<bool>("maybe");
  REQUIRE_FALSE(invalid_bool.has_value());
  CHECK_EQ(invalid_bool.error(), std::errc::invalid_argument);
}

TEST_CASE("reflex::parse_result::value_or_throw")
{
  CHECK_EQ(parse_or_throw<int>("7"), 7);

  CHECK_THROWS_AS(parse_or_throw<int>("7x"), parse_error);

  try
  {
    (void)parse_or_throw<int>("7x");
    FAIL("expected parse_or_throw to throw");
  }
  catch(parse_error const& e)
  {
    CHECK(std::string_view{e.what()} == "Parsing failed: Invalid argument");
  }
}

enum class [[=derive(Parse)]] Color { Red, Green, Blue };

TEST_CASE("reflex::core::parse: enum")
{
  CHECK(*parse<Color>("Red") == Color::Red);
  CHECK(*parse<Color>("Green") == Color::Green);
  CHECK(*parse<Color>("Blue") == Color::Blue);
}

enum class [[=derive(Parse, EnumFlags)]] Permission
{
  Read    = 1 << 0,
  Write   = 1 << 1,
  Execute = 1 << 2
};

TEST_CASE("reflex::core::parse: enum flags")
{
  CHECK(*parse<Permission>("Read") == Permission::Read);
  CHECK(*parse<Permission>("Write") == Permission::Write);
  CHECK(*parse<Permission>("Read|Execute") == (Permission::Read | Permission::Execute));
}