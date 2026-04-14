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
