#include <reflex/constant.hpp>
#include <reflex/error_code.hpp>

using namespace reflex;

struct test_error_codes : error::codes<test_error_codes>
{
  static constexpr std::string_view category = "Test errors";
  static constexpr code             success  = "Success";
  static constexpr code             failure  = "Failure";
};

#include <reflex/testing_main.hpp>

namespace error_code_tests
{

void base_test()
{
  CHECK_THAT(test_error_codes::make<^^test_error_codes::success>() == test_error_codes::success);
  CHECK_THAT(make_error_code<^^test_error_codes::success>() == test_error_codes::success);

  std::println("{}", test_error_codes{}.message_of(0));
  CHECK_THAT(test_error_codes{}.message_of(0)) == "Success";
  CHECK_THAT(test_error_codes{}.message_of(1)) == "Failure";
  CHECK_THAT(test_error_codes{}.make<^^test_error_codes::success>().message()) == "Success";
  CHECK_THAT(test_error_codes{}.make<^^test_error_codes::failure>().message()) == "Failure";
  try
  {
    raise<^^test_error_codes::failure>();
    ASSERT_THAT(false);
  }
  catch(std::system_error const& err)
  {
    using namespace std::string_view_literals;
    CHECK_THAT(err.what()) == "Failure"sv;
    CHECK_THAT(err.code().message()) == "Failure"sv;
    CHECK_THAT(err.code().category().name()) == "Test errors"sv;
  }
}
} // namespace error_code_tests