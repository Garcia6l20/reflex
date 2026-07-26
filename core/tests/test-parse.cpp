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
  auto invalid_int = parse<int>("z42x");
  REQUIRE_FALSE(invalid_int.has_value());
  CHECK_EQ(invalid_int.error(), std::errc::invalid_argument);

  auto invalid_bool = parse<bool>("maybe");
  REQUIRE_FALSE(invalid_bool.has_value());
  CHECK_EQ(invalid_bool.error(), std::errc::invalid_argument);
}

TEST_CASE("reflex::parse_result::value_or_throw")
{
  CHECK_EQ(parse_or_throw<int>("7"), 7);

  CHECK_THROWS_AS(parse_or_throw<int>("z7z"), parse_error);

  try
  {
    (void)parse_or_throw<int>("z7z");
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

namespace
{
  // A user-taught parsable type, to show parse_strict reaches beyond the
  // arithmetic overloads. Forwarding the inner end() is what lets it: reporting
  // the whole input would claim everything was consumed.
  struct Ratio
  {
    double value{};
    constexpr bool operator==(Ratio const&) const = default;
  };

  constexpr parse_result<Ratio> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<Ratio>) noexcept
  {
    auto inner = parse<double>(s);
    if(not inner)
    {
      return std::unexpected(std::errc::invalid_argument);
    }
    return {Ratio{*inner}, inner.end()};
  }
}

TEST_CASE("reflex::parse: permissive, and reports how far it got")
{
  // Integral and floating point behave the same way. They used to differ: the
  // floating point overload rejected a short parse and the integral one did not.
  CHECK_EQ(*parse<int>("12abc"), 12);
  CHECK_EQ(*parse<double>("1.5abc"), 1.5);
  CHECK_EQ(*parse<int>("12 "), 12);
  CHECK_EQ(*parse<double>("1.5 "), 1.5);

  // end() is the point of the permissive path: it is what lets a caller read a
  // sequence of fields out of one buffer.
  const auto text = "12abc"sv;
  CHECK_EQ(parse<int>(text).end(), text.data() + 2);
}

TEST_CASE("reflex::parse_strict: the whole input must be consumed")
{
  CHECK_EQ(*parse_strict<int>("12"), 12);
  CHECK_EQ(*parse_strict<double>("1.5"), 1.5);

  CHECK_FALSE(parse_strict<int>("12abc"));
  CHECK_FALSE(parse_strict<double>("1.5abc"));
  CHECK_FALSE(parse_strict<int>("12 "));
  CHECK_FALSE(parse_strict<double>("1.5 "));

  // The base prefixes still consume the whole value.
  CHECK_EQ(*parse_strict<int>("0x1f"), 31);
  CHECK_EQ(*parse_strict<int>("0b101"), 5);
  CHECK_FALSE(parse_strict<int>("0x1fz"));

  CHECK_EQ(*parse_strict<Ratio>("2.5"), Ratio{2.5});
  CHECK_FALSE(parse_strict<Ratio>("2.5abc"));

  CHECK_EQ(parse_strict_or<int>("12abc", -1), -1);
  CHECK_EQ(parse_strict_or<int>("12", -1), 12);
  CHECK_THROWS_AS(parse_strict_or_throw<int>("12abc"), parse_error);
  CHECK_EQ(parse_strict_or_else<int>("12abc", [](std::errc) { return -7; }), -7);
}
