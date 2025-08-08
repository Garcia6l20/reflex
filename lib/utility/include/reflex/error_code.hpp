#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>

#include <print>
#include <system_error>

namespace reflex
{
namespace error
{

template <typename Codes> struct error_category_t final : std::error_category
{
  [[nodiscard]] const char* name() const noexcept final
  {
    static constexpr auto category_mem = meta::member_named(^^Codes, "category");
    static_assert(category_mem != meta::null, "A (static constexpr std::string_view) 'category' member must be specified");
    return [:category_mem:].data();
  }
  [[nodiscard]] std::string message(int error) const noexcept final
  {
    return std::string{Codes::message_of(error)};
  }
};

template <typename Codes> struct code_type
{
  constant_string value;
  consteval code_type(auto const& v) : value{v}
  {
  }
};

template <typename Super> struct codes;

template <typename Codes> constexpr bool operator==(std::error_code const& err, error::code_type<Codes> const& code)
{
  return err.category() == Codes::template category_<typename Codes::tag> and err.value() == Codes::value_of(code);
}

template <typename Super> struct codes
{
  using code = code_type<Super>;

  struct tag {};

  template <typename>
  static constexpr auto codes_ =
      define_static_array(members_of(^^Super, meta::access_context::current())                                       //
                          | std::views::filter([](auto R) {//
                              return is_static_member(R) and meta::is_template_instance_of(type_of(R) , ^^code_type);
                            }) //
                          | std::views::transform(meta::constant_of));

  template <typename> static inline auto category_ = error::error_category_t<Super>{};

  static std::string_view message_of(int value) noexcept
  {
    int num = 0;
    template for(constexpr auto code_r : codes_<tag>)
    {
      if(num == value)
      {
        return *[:code_r:].value;
      }
      ++num;
    }
    return "<unknown>";
  }

  static int value_of(code const& c) noexcept
  {
    int num = 0;
    template for(constexpr auto code_r : codes_<tag>)
    {
      if(std::addressof(*c.value) == std::addressof(*[:code_r:].value))
      {
        return num;
      }
      ++num;
    }
    return -1;
  }

  template <meta::info code> static std::error_code make() noexcept
  {
    int num = 0;
    template for(constexpr auto c : codes_<tag>)
    {
      if constexpr(c == constant_of(code))
      {
        break;
      }
      ++num;
    }
    return std::error_code(num, category_<tag>);
  }

  template <meta::info code> static void raise()
  {
    throw std::system_error{make<code>()};
  }
};
} // namespace error

template <meta::info err>
auto make_error_code()
  requires(meta::is_subclass_of(parent_of(err), ^^error::codes))
{
  using ParentT = typename[:parent_of(err):];
  return ParentT::template make<err>();
}

template <meta::info err>
auto raise()
  requires(meta::is_subclass_of(parent_of(err), ^^error::codes))
{
  using ParentT = typename[:parent_of(err):];
  return ParentT::template raise<err>();
}

} // namespace reflex
