#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>

#include <system_error>

namespace reflex
{
namespace error
{

template <constant_string Name, typename ErrorCode> struct error_category_t final : std::error_category
{
  [[nodiscard]] const char* name() const noexcept final
  {
    return Name->data();
  }
  [[nodiscard]] std::string message(int error) const noexcept final
  {
    return std::string{ErrorCode::message_of(error)};
  }
};

template <int Value, constant_string Message> struct code
{
  static constexpr std::string_view message = Message;
  static constexpr auto             value   = Value;
};
} // namespace error

template <constant_string Category, typename Codes>
  requires(is_aggregate_type(^^Codes))
struct error_code : std::error_code
{
private:
  static constexpr auto codes_ = define_static_array(members_of(^^Codes, meta::access_context::current()) //
                                                     | std::views::filter(meta::is_static_member)         //
                                                     | std::views::transform(meta::constant_of));

public:
  static inline error::error_category_t<Category, error_code> category;
  static constexpr std::string_view                           message_of(int value) noexcept
  {
    template for(constexpr auto code_r : codes_)
    {
      constexpr auto code = [:code_r:];
      if(value == code.value)
      {
        return code.message;
      }
    }
    return "<unknown>";
  }

  template <auto code> static constexpr std::error_code make() noexcept
  {
    template for(constexpr auto code_r : codes_)
    {
      constexpr auto c = [:code_r:];
      if constexpr(c.value == code.value)
      {
        return std::error_code(c.value, category);
      }
    }
    return std::error_code(-1, category);
  }

  template <auto code> static constexpr void raise()
  {
    throw std::system_error{make<code>()};
  }
};

} // namespace reflex
