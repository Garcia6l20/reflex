#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/detail/annotations.hpp>
#include <reflex/qt/detail/metatype.hpp>

#include <QtCore/qtmochelpers.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace reflex::qt::detail
{
consteval std::size_t string_table_bytes(std::span<const constant_string> strings)
{
  std::size_t total = 0;
  for(auto const& s : strings)
  {
    total += s->size() + 1;
  }
  return total;
}

/** @brief the moc string table of @p Strings, shaped as `metaObjectData` consumes it
 *
 * `QtMocHelpers::StringRefStorage` cannot take its place: it deduces every
 * length from `std::extent_v` on a `char[N]` argument, which no runtime-sized
 * constant can supply. Only `writeTo`'s two offset lines are reproduced.
 */
template <auto const& Strings> struct string_storage
{
  static constexpr int         StringCount = int(Strings.size());
  static constexpr std::size_t StringSize  = string_table_bytes(Strings);

  constexpr void writeTo(uint (&offsets)[2 * StringCount], char (&data)[StringSize]) const noexcept
  {
    uint offset = 0;
    for(int i = 0; i < StringCount; ++i)
    {
      const std::string_view s = *Strings[i];
      for(std::size_t j = 0; j < s.size(); ++j)
      {
        data[offset + j] = s[j];
      }
      data[offset + s.size()] = '\0';
      offsets[2 * i]          = offset + sizeof(offsets);
      offsets[2 * i + 1]      = uint(s.size());
      offset += uint(s.size() + 1);
    }
  }
};

/** @brief the string table and the `QMetaObject` data blob of @p Super
 *
 * @p Tag is the per-class unique type moc calls `qt_meta_tag_*_t`; it selects
 * the metatype array the blob points at.
 */
template <typename Tag, typename Super> struct meta_strings
{
  static constexpr auto invocables =
      define_static_array(meta::member_functions_annotated_with(^^Super,
                                                                ^^detail::invocable,
                                                                meta::access_context::unchecked()));

  static constexpr auto properties = define_static_array(
      meta::nonstatic_data_members_annotated_with(^^Super,
                                                  ^^detail::property,
                                                  meta::access_context::unchecked()));

  static constexpr auto invocable_count = invocables.size();
  static constexpr auto property_count  = properties.size();

  static constexpr auto custom_types = [] consteval
  {
    std::vector<meta::info> types;
    auto                    try_push = [&types](meta::info R)
    {
      const auto T = normalized_type_of(R);
      if(static_meta_type_id_of(T) == custom_type and not std::ranges::contains(types, T))
      {
        types.push_back(T);
      }
    };

    for(auto p : properties)
    {
      try_push(type_of(p));
    }
    for(auto fn : invocables)
    {
      for(auto param : parameters_of(fn))
      {
        try_push(type_of(param));
      }
      try_push(return_type_of(fn));
    }
    return define_static_array(types);
  }();

  static constexpr auto classinfo_strings = [] consteval
  {
    std::vector<constant_string> list;

    for(auto a : meta::annotations_of_with(^^Super, ^^qt::classinfo))
    {
      const auto entry = extract<qt::classinfo>(constant_of(a));
      list.push_back(entry.key);
      list.push_back(entry.value);
    }
    return define_static_array(list);
  }();

  static consteval std::string_view parameter_name_of(meta::info R)
  {
    return has_identifier(R) ? identifier_of(R) : std::string_view{};
  }

  static constexpr auto strings = [] consteval
  {
    std::vector<constant_string> list;
    auto                         push = [&list](std::string_view s)
    {
      for(auto const& known : list)
      {
        if(*known == s)
        {
          return;
        }
      }
      list.push_back(s);
    };

    push(identifier_of(^^Super));
    for(auto const& s : classinfo_strings)
    {
      push(*s);
    }
    push("");
    for(auto fn : invocables)
    {
      push(identifier_of(fn));
      for(auto param : parameters_of(fn))
      {
        push(parameter_name_of(param));
      }
    }
    for(auto p : properties)
    {
      push(identifier_of(p));
    }
    for(auto t : custom_types)
    {
      push(normalized_type_name(t));
    }
    return define_static_array(list);
  }();

  static constexpr std::size_t classinfo_count = classinfo_strings.size() / 2;

  /** @brief where @p s sits in the string table
   *
   * moc emits each distinct string once and indexes it from everywhere it is
   * needed, so the table is deduplicated and every index is a lookup.
   */
  static consteval uint index_of(std::string_view s)
  {
    for(std::size_t i = 0; i < strings.size(); ++i)
    {
      if(*strings[i] == s)
      {
        return uint(i);
      }
    }
    return uint(strings.size());
  }

  static constexpr uint empty_string_index = index_of("");

  /** @brief @p R's `QMetaType` id, or `custom_type | <string index>` when it has none */
  static consteval uint meta_type_id_of(meta::info R)
  {
    const auto T  = normalized_type_of(R);
    const auto id = uint(static_meta_type_id_of(T));
    if(id != uint(custom_type))
    {
      return id;
    }
    return id | index_of(normalized_type_name(T));
  }

  static consteval uint access_flags_of(meta::info R)
  {
    namespace QMC = QtMocConstants;
    if(is_public(R))
    {
      return QMC::AccessPublic;
    }
    if(is_protected(R))
    {
      return QMC::AccessProtected;
    }
    return QMC::AccessPrivate;
  }

  template <std::size_t I> static consteval auto method_data_of()
  {
    constexpr auto fn         = invocables[I];
    using signature_type      = [:meta::signature_of<fn>():];
    using data_type           = QtMocHelpers::FunctionData<signature_type, QtMocConstants::MethodMethod>;
    constexpr auto parameters = define_static_array(parameters_of(fn));

    typename data_type::ParametersArray args{};
    for(std::size_t i = 0; i < parameters.size(); ++i)
    {
      args[i] = {meta_type_id_of(type_of(parameters[i])),
                 index_of(parameter_name_of(parameters[i]))};
    }
    return data_type(index_of(identifier_of(fn)),
                     empty_string_index,
                     access_flags_of(fn),
                     meta_type_id_of(return_type_of(fn)),
                     args);
  }

  template <std::size_t I> static consteval auto property_data_of()
  {
    namespace QMC = QtMocConstants;

    constexpr auto p    = properties[I];
    using property_type = [:type_of(p):];
    constexpr auto spec = meta::annotation_value_of_with<detail::property>(p);

    uint flags = QMC::DefaultPropertyFlags;
    if constexpr(spec.writable())
    {
      flags |= QMC::Writable;
    }
    return QtMocHelpers::PropertyData<property_type>(index_of(identifier_of(p)),
                                                     meta_type_id_of(type_of(p)),
                                                     flags);
  }

  static consteval auto create_meta_objectdata()
  {
    namespace QMC = QtMocConstants;

    constexpr string_storage<strings> qt_stringData{};

    const auto qt_methods = []<std::size_t... I>(std::index_sequence<I...>)
    {
      return QtMocHelpers::UintData{method_data_of<I>()...};
    }(std::make_index_sequence<invocable_count>());

    const auto qt_properties = []<std::size_t... I>(std::index_sequence<I...>)
    {
      return QtMocHelpers::UintData{property_data_of<I>()...};
    }(std::make_index_sequence<property_count>());

    const auto qt_classinfo = []<std::size_t... I>(std::index_sequence<I...>)
    {
      if constexpr(classinfo_count == 0)
      {
        return QtMocHelpers::ClassInfos<0>{};
      }
      else
      {
        return QtMocHelpers::ClassInfos<classinfo_count>({
            std::array<uint, 2>{index_of(*classinfo_strings[I * 2]),
                                index_of(*classinfo_strings[I * 2 + 1])}...
        });
      }
    }(std::make_index_sequence<classinfo_count>());

    QtMocHelpers::UintData qt_enums{};
    QtMocHelpers::UintData qt_constructors{};

    return QtMocHelpers::metaObjectData<Super, Tag>(QMC::PropertyAccessInStaticMetaCall,
                                                    qt_stringData,
                                                    qt_methods,
                                                    qt_properties,
                                                    qt_enums,
                                                    qt_constructors,
                                                    qt_classinfo);
  }
};
}
