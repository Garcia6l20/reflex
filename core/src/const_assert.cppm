export module reflex.core:const_assert;

import std;

export namespace reflex
{
struct assertion_error : std::logic_error
{
  using std::logic_error::logic_error;
};

consteval void const_assert(bool                 b,
                            std::string_view     description = "",
                            std::source_location loc         = std::source_location::current())
{
  if(!b)
  {
    using namespace std::string_literals;
    std::string message = "Assertion failed at: "s + loc.file_name() + ":";

    char line_str[16] = "";
    std::to_chars(line_str, line_str + sizeof(line_str), loc.line());
    message += std::string_view{line_str};
    message += " in function: "s + loc.function_name();
    if(!description.empty())
    {
      message += ": \""s + description + "\"";
    }

    throw assertion_error(message);
  }
}
} // namespace reflex