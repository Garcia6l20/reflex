#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cstdint>
#include <utility>

#include <reflex/tag_invoke.hpp>
#endif

#include <reflex/serde/annotations.hpp>
#include <reflex/serde/comments.hpp>
#include <reflex/serde/mmap_input_stream.hpp>
#include <reflex/serde/object_visit.hpp>
#include <reflex/serde/poly.hpp>

/// serializers registry
REFLEX_EXPORT namespace reflex::serde::ser
{}

/// deserializers registry
REFLEX_EXPORT namespace reflex::serde::de
{}

REFLEX_EXPORT namespace reflex::serde
{
  constexpr struct _optional
  {
  } optional;

  consteval auto serializers()
  {
    constexpr auto ctx = std::meta::access_context::current();
    return members_of(^^reflex::serde::ser, ctx);
  }

  constexpr auto with_serializer(std::string_view file_format, auto out, auto&& fn)
  {
    template for(constexpr auto entry : define_static_array(serializers()))
    {
      if(identifier_of(entry) == file_format)
      {
        constexpr auto ser_template = extract<std::meta::info>(entry);
        constexpr auto ser_type     = substitute(ser_template, {remove_cvref(type_of(^^out))});
        typename[:ser_type:] ser{out};
        fn(ser);
        return;
      }
      // std::println("Checked serializer: {}", identifier_of(entry));
    }
    throw runtime_error("No serializer found for file format '{}'", file_format);
  }

  consteval auto deserializers()
  {
    constexpr auto ctx = std::meta::access_context::current();
    return members_of(^^reflex::serde::de, ctx);
  }

  constexpr auto with_deserializer(std::string_view file_format, auto rng, auto&& fn)
  {
    template for(constexpr auto entry : define_static_array(deserializers()))
    {
      if(identifier_of(entry) == file_format)
      {
        constexpr auto de_template = extract<std::meta::info>(entry);
        auto           begin       = rng.begin();
        auto           end         = rng.end();
        constexpr auto de_type     = substitute(de_template, {remove_cvref(type_of(^^begin))});
        typename[:de_type:] de{begin, end};
        fn(de);
        return;
      }
      // std::println("Checked deserializer: {}", identifier_of(entry));
    }
    throw runtime_error("No deserializer found for file format '{}'", file_format);
  }

  namespace detail
  {
  template <typename S> inline constexpr bool ext_json_serializer_v = false;

  template <typename S>
  concept ext_json_serializer_c = ext_json_serializer_v<std::remove_cvref_t<S>>;

  template <typename T> inline constexpr bool ext_json_type_v = false;

  template <typename Var> struct ext_json_reader
  {
    static constexpr bool enabled = false;
  };

  template <typename Var> Var relaxed_integer(std::int64_t value)
  {
    static_assert(
        Var::template can_hold<std::int32_t>() and Var::template can_hold<std::int64_t>(),
        "an ext_json_reader value type must hold std::int32_t and std::int64_t");
    if(std::in_range<std::int32_t>(value))
    {
      return Var{static_cast<std::int32_t>(value)};
    }
    return Var{value};
  }
  }

  inline constexpr struct serialize_cpo : customization_point_object
  {
  } serialize;
  inline constexpr struct deserialize_cpo : customization_point_object
  {
  } deserialize;

} // namespace reflex::serde
