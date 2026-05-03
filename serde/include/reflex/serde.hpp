#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/serde/annotations.hpp>
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

  constexpr auto with_serializer(std::string_view file_format, auto&& fn)
  {
    template for(constexpr auto entry : define_static_array(serializers()))
    {
      if(identifier_of(entry) == file_format)
      {
        using T = typename[:entry:];
        fn(T{});
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

  constexpr auto with_deserializer(std::string_view file_format, auto&& fn)
  {
    template for(constexpr auto entry : define_static_array(deserializers()))
    {
      if(identifier_of(entry) == file_format)
      {
        using T = typename[:entry:];
        fn(T{});
        return;
      }
      // std::println("Checked deserializer: {}", identifier_of(entry));
    }
    throw runtime_error("No deserializer found for file format '{}'", file_format);
  }

  inline constexpr struct serialize_cpo : customization_point_object
  {
  } serialize;
  inline constexpr struct deserialize_cpo : customization_point_object
  {
  } deserialize;

} // namespace reflex::serde
