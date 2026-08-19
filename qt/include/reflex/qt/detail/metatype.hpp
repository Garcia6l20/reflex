#pragma once

#include <reflex/meta.hpp>
#include <reflex/qt/detail/version.hpp>

#include <QtCore/qflags.h>
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

/** @brief @p R's qualified name, an alias left unexpanded
 *
 * `display_string_of` on an alias answers `Qt::Alignment {aka QFlags<...>}`, so
 * the name is rebuilt from the enclosing scope and the identifier.
 */
consteval std::string qualified_name_of(meta::info R)
{
  const auto scope = meta::parent_of(R);
  if(meta::is_namespace(scope) and not meta::has_identifier(scope))
  {
    return std::string{meta::identifier_of(R)};
  }
  return std::string{meta::display_string_of(scope)} + "::" + std::string{meta::identifier_of(R)};
}

/** @brief the qualified alias the `QFlags` specialization @p R is declared as
 *
 * moc echoes the alias the author wrote, and `type_of` on a data member answers
 * the specialization rather than the alias it was spelled with, so the spelling
 * has to be found again. It sits next to the enumeration, which is where
 * `Q_DECLARE_FLAGS` puts it. Empty when that scope declares none.
 */
consteval std::string flags_alias_name_of(meta::info R)
{
  if(not meta::is_template_instance_of(R, ^^QFlags))
  {
    return {};
  }
  const auto scope = meta::parent_of(meta::dealias(meta::template_arguments_of(R)[0]));
  for(auto member : meta::members_of(scope, meta::access_context::unchecked()))
  {
    if(meta::is_type_alias(member) and meta::dealias(member) == R)
    {
      return qualified_name_of(member);
    }
  }
  return {};
}

/** @brief whether @p R names an enumeration or a `QFlags` specialization */
consteval bool is_enum_or_flags(meta::info R)
{
  const auto T = normalized_type_of(R);
  return meta::is_enum_type(T) or meta::is_template_instance_of(T, ^^QFlags);
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
