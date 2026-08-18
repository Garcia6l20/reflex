#pragma once

#include <reflex/meta.hpp>
#include <reflex/qt/detail/version.hpp>

#include <QtCore/qmetatype.h>

#include <bit>
#include <ranges>
#include <string>

namespace reflex::qt::detail
{
inline constexpr auto custom_type = std::bit_cast<QMetaType::Type>(0x8000'0000u);

/** @brief @p R stripped of its alias, reference and top-level const */
consteval meta::info normalized_type_of(meta::info R)
{
  return meta::remove_const(meta::remove_reference(meta::dealias(R)));
}

/** @brief the type name moc would emit for @p R, spaces removed */
consteval std::string normalized_type_name(meta::info R)
{
  return meta::display_string_of(normalized_type_of(R))
       | std::views::filter([](char c) { return c != ' '; })
       | std::ranges::to<std::string>();
}

/** @brief @p R's built-in `QMetaType` id, or `custom_type` when it has none */
consteval QMetaType::Type static_meta_type_id_of(meta::info R)
{
  const auto T = normalized_type_of(R);
  if(false)
  {
  }
#define X(__enum_mem, __id, __type)                              \
  else if(normalized_type_of(^^__type) == T)                     \
  {                                                              \
    return QMetaType::__enum_mem;                                \
  }
  QT_FOR_EACH_STATIC_TYPE(X)
#undef X
  return custom_type;
}
}
