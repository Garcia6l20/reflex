#include <doctest/doctest.h>

#include <reflex/core.hpp>

TEST_CASE("reflex::parse: base types")
{
  CHECK(reflex::parser<int>{}("42").value() == 42);
  CHECK(reflex::parser<int>{}("-17").value() == -17);
  CHECK(reflex::parser<int>{}("0x2a").value() == 42);
  CHECK(reflex::parser<int>{}("0b101010").value() == 42);
  CHECK(reflex::parser<double>{}("3.14").value() == doctest::Approx(3.14));
  CHECK(reflex::parser<std::string>{}("hello").value() == "hello");
}

TEST_CASE("reflex::parse: failures return error codes")
{
  auto invalid_int = reflex::parse<int>("42x");
  REQUIRE_FALSE(invalid_int.has_value());
  CHECK_EQ(invalid_int.error().value(), std::make_error_code(std::errc::invalid_argument).value());

  auto invalid_bool = reflex::parse<bool>("maybe");
  REQUIRE_FALSE(invalid_bool.has_value());
  CHECK_EQ(invalid_bool.error().value(), std::make_error_code(std::errc::invalid_argument).value());
}

TEST_CASE("reflex::parse_result::value_or_throw")
{
  CHECK_EQ(reflex::parse_or_throw<int>("7"), 7);

  CHECK_THROWS_AS(reflex::parse_or_throw<int>("7x"), reflex::parse_error);

  try
  {
    (void)reflex::parse_or_throw<int>("7x");
    FAIL("expected parse_or_throw to throw");
  }
  catch(reflex::parse_error const& e)
  {
    CHECK(std::string_view{e.what()}.contains("Parsing '7x' as 'int' failed:"));
  }
}
