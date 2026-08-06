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
#include <reflex/serde/yaml_value.hpp>
#endif

#include <reflex/serde/detail/io.hpp>

REFLEX_EXPORT namespace reflex::serde::yaml
{
  template <typename OutputIt> class serializer : public serde::detail::serializer_base<OutputIt>
  {
  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "YAML";
  };

  template <typename... TArgs>
  serializer(std::basic_string<TArgs...> & out)
      -> serializer<std::back_insert_iterator<std::basic_string<TArgs...>>>;
  serializer(std::ofstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  serializer(std::ostringstream & out) -> serializer<std::ostreambuf_iterator<char>>;

  template <std::input_iterator InputIt>
  class deserializer : public serde::detail::subrange_deserializer<InputIt>
  {
    using base = serde::detail::subrange_deserializer<InputIt>;
    using base::cursor_;

  public:
    using base::at_end;
    using base::base;

    // makes load() without an explicit type read a yaml::value
    using default_load_type = yaml::value;
  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

} // namespace reflex::serde::yaml

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto yaml = ^^reflex::serde::yaml::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto yaml = ^^reflex::serde::yaml::deserializer;
}
