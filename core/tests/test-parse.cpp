#include <doctest/doctest.h>

#include <reflex/core.hpp>

TEST_CASE("reflex::parse: base types")
{
  CHECK(reflex::parser<int>{}("42") == 42);
  CHECK(reflex::parser<int>{}("-17") == -17);
  CHECK(reflex::parser<int>{}("0x2a") == 42);
  CHECK(reflex::parser<int>{}("0b101010") == 42);
  CHECK(reflex::parser<double>{}("3.14") == doctest::Approx(3.14));
  CHECK(reflex::parser<std::string>{}("hello") == "hello");
}
