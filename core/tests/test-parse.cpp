#include <doctest/doctest.h>

#include <reflex/core.hpp>

using namespace reflex;

TEST_CASE("reflex::parse: base types")
{
  CHECK(parser<int>{}("42").value() == 42);
  CHECK(parser<int>{}("-17").value() == -17);
  CHECK(parser<int>{}("0x2a").value() == 42);
  CHECK(parser<int>{}("0b101010").value() == 42);
  CHECK(parser<double>{}("3.14").value() == doctest::Approx(3.14));
  CHECK(parser<std::string>{}("hello").value() == "hello");
}

TEST_CASE("reflex::parse: failures return error codes")
{
  auto invalid_int = parse<int>("42x");
  REQUIRE_FALSE(invalid_int.has_value());
  CHECK_EQ(invalid_int.error().value(), std::make_error_code(std::errc::invalid_argument).value());

  auto invalid_bool = parse<bool>("maybe");
  REQUIRE_FALSE(invalid_bool.has_value());
  CHECK_EQ(invalid_bool.error().value(), std::make_error_code(std::errc::invalid_argument).value());
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
    CHECK(std::string_view{e.what()}.contains("Parsing '7x' as 'int' failed:"));
  }
}

enum class [[=derive(Parse)]] Color { Red, Green, Blue };

TEST_CASE("reflex::core::parse: enum")
{
  CHECK(parser<Color>{}("Red").value() == Color::Red);
  CHECK(parser<Color>{}("Green").value() == Color::Green);
  CHECK(parser<Color>{}("Blue").value() == Color::Blue);
}
