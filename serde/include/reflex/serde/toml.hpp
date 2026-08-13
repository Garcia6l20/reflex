#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cmath>
#include <cstring>

#include <reflex/format.hpp>
#include <reflex/heapless/string.hpp>
#include <reflex/parse.hpp>
#include <reflex/serde.hpp>
#include <reflex/serde/toml_value.hpp>
#endif

#include <reflex/serde/detail/escape.hpp>
#include <reflex/serde/detail/io.hpp>
#include <reflex/serde/detail/line_cursor.hpp>
#include <reflex/serde/detail/text.hpp>

REFLEX_EXPORT namespace reflex::serde::toml
{
  namespace detail
  {
  } // namespace detail

  template <typename OutputIt> class serializer : public serde::detail::serializer_base<OutputIt>
  {
  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "TOML";
  };

  template <typename... TArgs>
  serializer(std::basic_string<TArgs...> & out)
      -> serializer<std::back_insert_iterator<std::basic_string<TArgs...>>>;
  serializer(std::ofstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  serializer(std::ostringstream & out) -> serializer<std::ostreambuf_iterator<char>>;

  template <std::input_iterator InputIt>
  class deserializer : public serde::detail::line_cursor<InputIt>
  {
  public:
    using serde::detail::line_cursor<InputIt>::line_cursor;

    static constexpr std::string_view format_name = "TOML";

    // makes load() without an explicit type read a toml::value
    using default_load_type = toml::value;
  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

} // namespace reflex::serde::toml

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto toml = ^^reflex::serde::toml::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto toml = ^^reflex::serde::toml::deserializer;
}
