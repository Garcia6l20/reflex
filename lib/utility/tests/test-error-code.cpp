#include <reflex/error_code.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace reflex;

struct test_error_codes
{
  static constexpr error::code<0, "Success"> success;
  static constexpr error::code<-1, "Failure"> failure;
};
using test_error = error_code<"Test errors", test_error_codes>;

TEST_CASE("easy error codes")
{
  std::println("{}", test_error::message_of(0));
  STATIC_REQUIRE(test_error::message_of(0) == "Success");
  STATIC_REQUIRE(test_error::message_of(-1) == "Failure");
  REQUIRE(test_error::make<test_error_codes::success>().message() == "Success");
  REQUIRE(test_error::make<test_error_codes::failure>().message() == "Failure");
  try
  {
    test_error::raise<test_error_codes::failure>();
    FAIL("not thrown");
  }
  catch(std::system_error const& err)
  {
    using namespace std::string_view_literals;
    REQUIRE(err.what() == "Failure"sv);
    REQUIRE(err.code().message() == "Failure"sv);
    REQUIRE(err.code().category().name() == "Test errors"sv);
  }
}
