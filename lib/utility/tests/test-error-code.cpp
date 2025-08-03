#include <reflex/error_code.hpp>

using namespace reflex;

struct test_error_codes
{
  static constexpr error::code<0, "Success">  success;
  static constexpr error::code<-1, "Failure"> failure;
};
using test_error = error_code<"Test errors", test_error_codes>;

#include <reflex/testing_main.hpp>

namespace error_code_tests
{

void base_test()
{
  std::println("{}", test_error::message_of(0));
  ASSERT_THAT(test_error::message_of(0)) == "Success";
  ASSERT_THAT(test_error::message_of(-1)) == "Failure";
  ASSERT_THAT(test_error::make<test_error_codes::success>().message()) == "Success";
  ASSERT_THAT(test_error::make<test_error_codes::failure>().message()) == "Failure";
  try
  {
    test_error::raise<test_error_codes::failure>();
    ASSERT_THAT(false);
  }
  catch(std::system_error const& err)
  {
    using namespace std::string_view_literals;
    ASSERT_THAT(err.what()) == "Failure"sv;
    ASSERT_THAT(err.code().message()) == "Failure"sv;
    ASSERT_THAT(err.code().category().name()) == "Test errors"sv;
  }
}
} // namespace error_code_tests