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

TEST_CASE("reflex::parse: an optional target holds what was parsed")
{
  // An optional says the value may be absent from the input. A value that is
  // there parses into an engaged optional, not into nothing.
  CHECK_EQ(*parse<std::optional<int>>("12"), std::optional{12});
  CHECK_EQ(*parse_strict<std::optional<int>>("12"), std::optional{12});
  CHECK_EQ(*parse_strict<std::optional<std::string>>("hello"), std::optional{std::string{"hello"}});

  CHECK_FALSE(parse<std::optional<int>>("abc"));
  CHECK_FALSE(parse_strict<std::optional<int>>("12abc"));

  // The spec reaches the underlying type. A time_point is only reachable
  // through one, so dropping it here would leave it unparseable.
  using tp = std::chrono::sys_time<std::chrono::nanoseconds>;
  static_assert(spec_parsable_c<std::optional<tp>>);
  CHECK_EQ(*parse<std::optional<tp>, "%Y-%m-%d">("2026-07-27"),
           std::optional{*parse<tp, "%Y-%m-%d">("2026-07-27")});
}

TEST_CASE("reflex::parse: a duration reads its unit off the suffix")
{
  using namespace std::chrono;

  // std::chrono has a parse of its own, so the name has to be qualified inside
  // any scope that pulls that namespace in.
  SUBCASE("every suffix std::chrono prints")
  {
    CHECK_EQ(*parse_strict<nanoseconds>("5ns"), 5ns);
    CHECK_EQ(*parse_strict<nanoseconds>("5us"), 5us);
    CHECK_EQ(*parse_strict<nanoseconds>("5ms"), 5ms);
    CHECK_EQ(*parse_strict<nanoseconds>("5s"), 5s);
    CHECK_EQ(*parse_strict<nanoseconds>("5min"), 5min);
    CHECK_EQ(*parse_strict<nanoseconds>("5h"), 5h);
  }

  SUBCASE("the longest suffix wins")
  {
    // "ms" must never read as "m" and "min" must never read as "m", which is
    // what a naive first-match table gets wrong.
    CHECK_EQ(*parse_strict<nanoseconds>("1ms"), 1ms);
    CHECK_EQ(*parse_strict<nanoseconds>("1min"), 1min);
    CHECK_EQ(*parse_strict<nanoseconds>("1s"), 1s);
  }

  SUBCASE("a bare number is in the destination's own units")
  {
    // The only rule that stays consistent across destination types. Reading a
    // bare number as seconds regardless would make parse<nanoseconds>("5") mean
    // five billion.
    CHECK_EQ(*parse_strict<milliseconds>("100"), 100ms);
    CHECK_EQ(*parse_strict<seconds>("100"), 100s);
    CHECK_EQ(*parse_strict<nanoseconds>("100"), 100ns);
    CHECK_EQ(*parse_strict<minutes>("2"), 2min);
  }

  SUBCASE("a fractional value survives into a floating representation")
  {
    CHECK_EQ(*parse_strict<duration<double, std::milli>>("1.5ms"), duration<double, std::milli>{1.5});
    CHECK_EQ(*parse_strict<duration<double, std::milli>>("1.5"), duration<double, std::milli>{1.5});
    CHECK_EQ(*parse_strict<nanoseconds>("1.5ms"), 1500000ns);
    // And truncates into an integral one exactly as a duration_cast would.
    CHECK_EQ(*parse_strict<milliseconds>("1500us"), 1ms);
  }

  SUBCASE("a duration is signed, so a negative one parses")
  {
    CHECK_EQ(*parse_strict<milliseconds>("-5ms"), -5ms);
    CHECK_EQ(*parse_strict<duration<double>>("-0.5s"), duration<double>{-0.5});
  }

  SUBCASE("end() reports what was consumed")
  {
    // Not constexpr: the numeric part goes through from_chars for double, which
    // libstdc++ does not offer as a constant expression.
    const auto input  = "100ms rest";
    const auto result = reflex::parse<milliseconds>(input);
    REQUIRE(result.has_value());
    CHECK(result.value() == 100ms);
    CHECK(result.end() == input + 5);

    // Which is what lets parse_strict refuse the leftovers.
    CHECK_FALSE(parse_strict<milliseconds>("100ms rest"));
    CHECK_FALSE(parse_strict<milliseconds>("1mss"));
    CHECK_FALSE(parse_strict<milliseconds>("1 ms"));
  }

  SUBCASE("nothing numeric is not a duration")
  {
    CHECK_FALSE(reflex::parse<milliseconds>(""));
    CHECK_FALSE(reflex::parse<milliseconds>("ms"));
    CHECK_FALSE(reflex::parse<milliseconds>("quickly"));
  }

  SUBCASE("an optional duration is what a command line option holds")
  {
    static_assert(parsable_c<std::optional<milliseconds>>);
    CHECK_EQ(*parse_strict<std::optional<milliseconds>>("250us"), std::optional{0ms});
    CHECK_EQ(*parse_strict<std::optional<milliseconds>>("2s"), std::optional{2000ms});
    CHECK_FALSE(parse_strict<std::optional<milliseconds>>("2 s"));
  }

  SUBCASE("a duration is not a time point and a time point is not a duration")
  {
    // The two concepts have to stay disjoint or one of the tag_invoke overloads
    // becomes ambiguous for the other's type.
    static_assert(duration_c<milliseconds>);
    static_assert(not duration_c<sys_time<nanoseconds>>);
    static_assert(not time_point_c<milliseconds>);
    static_assert(not duration_c<int>);
    static_assert(not duration_c<std::string>);
  }
}
