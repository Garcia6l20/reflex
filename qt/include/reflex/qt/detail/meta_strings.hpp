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
#include <string>
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

/** @brief what a method table entry was built from */
enum class method_kind : unsigned
{
  signal_member,
  notifier,
  slot,
  invocable
};

/** @brief one row of the `QMetaObject` method table
 *
 * Qt allocates one row per default-argument arity, so a member with defaults
 * contributes several rows: the longest signature first, then one clone per
 * omitted trailing argument.
 */
struct method_entry
{
  meta::info  member;
  std::size_t arity;
  method_kind kind;
  bool        cloned;
};

consteval uint moc_method_flag_of(method_kind kind)
{
  switch(kind)
  {
  case method_kind::signal_member:
  case method_kind::notifier:
    return QtMocConstants::MethodSignal;
  case method_kind::slot:
    return QtMocConstants::MethodSlot;
  case method_kind::invocable:
    return QtMocConstants::MethodMethod;
  }
  return QtMocConstants::MethodMethod;
}

consteval std::string notifier_name_of(meta::info Property)
{
  return std::string{identifier_of(Property)} + "Changed";
}

/** @brief the string table and the `QMetaObject` data blob of @p Super
 *
 * @p Tag is the per-class unique type moc calls `qt_meta_tag_*_t`; it selects
 * the metatype array the blob points at.
 */
template <typename Tag, typename Super> struct meta_strings
{
  static constexpr bool is_object =
      meta::is_subclass_of(^^Super, ^^qt::object, meta::access_context::unchecked());

  static constexpr auto signal_members = [] consteval
  {
    if constexpr(is_object)
    {
      return define_static_array(
          meta::nonstatic_data_members_of(^^Super, meta::access_context::unchecked())
          | std::views::filter([](meta::info m) {
              return meta::is_template_instance_of(dealias(remove_const(type_of(m))),
                                                   ^^detail::signal_decl);
            }));
    }
    else
    {
      return std::span<const meta::info>{};
    }
  }();

  static constexpr auto slot_members = [] consteval
  {
    if constexpr(is_object)
    {
      return define_static_array(
          meta::member_functions_annotated_with(^^Super,
                                                ^^detail::slot,
                                                meta::access_context::unchecked()));
    }
    else
    {
      return std::span<const meta::info>{};
    }
  }();

  static constexpr auto invocables =
      define_static_array(meta::member_functions_annotated_with(^^Super,
                                                                ^^detail::invocable,
                                                                meta::access_context::unchecked()));

  static constexpr auto properties = [] consteval
  {
    validate_properties(^^Super);
    return define_static_array(
        meta::nonstatic_data_members_annotated_with(^^Super,
                                                    ^^detail::property,
                                                    meta::access_context::unchecked()));
  }();

  static constexpr auto invocable_count = invocables.size();
  static constexpr auto property_count  = properties.size();

  static constexpr auto methods = [] consteval
  {
    std::vector<method_entry> list;
    auto                      push = [&list](meta::info member,
                                             method_kind kind,
                                             std::size_t max_arity,
                                             std::size_t min_arity)
    {
      for(std::size_t n = max_arity + 1; n-- > min_arity;)
      {
        list.push_back({member, n, kind, n != max_arity});
      }
    };

    for(auto s : signal_members)
    {
      const auto total = parameters_of(call_function_of(s)).size();
      push(s, method_kind::signal_member, total, total - signal_default_count_of(s));
    }
    if constexpr(is_object)
    {
      for(auto p : properties)
      {
        list.push_back({p, 0, method_kind::notifier, false});
      }
    }
    for(auto fn : slot_members)
    {
      push(fn, method_kind::slot, parameters_of(fn).size(), meta::min_arity_of(fn));
    }
    for(auto fn : invocables)
    {
      push(fn, method_kind::invocable, parameters_of(fn).size(), meta::min_arity_of(fn));
    }
    return std::define_static_array(list);
  }();

  static constexpr auto method_count = methods.size();

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
    for(auto const& e : methods)
    {
      if(e.kind == method_kind::notifier or e.cloned)
      {
        continue;
      }
      const auto fn = call_function_of(e.member);
      for(auto param : parameters_of(fn))
      {
        try_push(type_of(param));
      }
      try_push(call_return_type_of(e.member));
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
    for(auto const& e : methods)
    {
      if(e.kind == method_kind::notifier)
      {
        push(notifier_name_of(e.member));
        continue;
      }
      push(identifier_of(e.member));
      for(auto param : parameters_of(call_function_of(e.member)))
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

  /** @brief the method index of @p name's notify signal, or `method_count` */
  static consteval std::size_t notifier_index_of(std::string_view name)
  {
    for(std::size_t i = 0; i < methods.size(); ++i)
    {
      if(methods[i].kind == method_kind::notifier and identifier_of(methods[i].member) == name)
      {
        return i;
      }
    }
    return methods.size();
  }

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

  static consteval uint method_flags_of(meta::info R, bool cloned)
  {
    namespace QMC = QtMocConstants;
    uint flags    = access_flags_of(R);
    if(cloned)
    {
      flags |= QMC::MethodCloned;
    }
    if(is_function(R) and meta::is_const(R))
    {
      flags |= QMC::MethodIsConst;
    }
    return flags;
  }

  template <std::size_t I> static consteval auto method_data_of()
  {
    namespace QMC = QtMocConstants;

    if constexpr(methods[I].kind == method_kind::notifier)
    {
      using data_type = QtMocHelpers::FunctionData<void(), QMC::MethodSignal>;
      return data_type(index_of(notifier_name_of(methods[I].member)),
                       empty_string_index,
                       QMC::AccessPublic,
                       QMetaType::Void);
    }
    else
    {
      constexpr auto fn         = call_function_of(methods[I].member);
      using signature_type      = [:meta::signature_of<fn, methods[I].arity>():];
      using data_type           = QtMocHelpers::FunctionData<signature_type,
                                                             moc_method_flag_of(methods[I].kind)>;
      constexpr auto parameters = define_static_array(parameters_of(fn));

      typename data_type::ParametersArray args{};
      for(std::size_t i = 0; i < methods[I].arity; ++i)
      {
        args[i] = {meta_type_id_of(type_of(parameters[i])),
                   index_of(parameter_name_of(parameters[i]))};
      }
      return data_type(index_of(identifier_of(methods[I].member)),
                       empty_string_index,
                       method_flags_of(methods[I].member, methods[I].cloned),
                       meta_type_id_of(call_return_type_of(methods[I].member)),
                       args);
    }
  }

  template <std::size_t I> static consteval auto property_data_of()
  {
    namespace QMC = QtMocConstants;

    constexpr auto p    = properties[I];
    using property_type = [:type_of(p):];
    constexpr auto spec = property_spec_of(p);

    uint flags = QMC::DefaultPropertyFlags;
    if constexpr(not spec.read)
    {
      flags &= ~uint(QMC::Readable);
    }
    if constexpr(spec.write)
    {
      flags |= QMC::Writable;
    }
    if constexpr(spec.constant)
    {
      flags |= QMC::Constant;
    }
    if constexpr(spec.final)
    {
      flags |= QMC::Final;
    }
    if constexpr(spec.required)
    {
      flags |= QMC::Required;
    }
    constexpr uint notify_id =
        is_object ? uint(notifier_index_of(identifier_of(p))) : uint(-1);
    return QtMocHelpers::PropertyData<property_type>(index_of(identifier_of(p)),
                                                     meta_type_id_of(type_of(p)),
                                                     flags,
                                                     notify_id);
  }

  static consteval auto create_meta_objectdata()
  {
    namespace QMC = QtMocConstants;

    constexpr string_storage<strings> qt_stringData{};

    const auto qt_methods = []<std::size_t... I>(std::index_sequence<I...>)
    {
      return QtMocHelpers::UintData{method_data_of<I>()...};
    }(std::make_index_sequence<method_count>());

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

    constexpr uint object_flags = is_object ? uint(QMC::MetaObjectFlag{})
                                            : uint(QMC::PropertyAccessInStaticMetaCall);

    return QtMocHelpers::metaObjectData<Super, Tag>(object_flags,
                                                    qt_stringData,
                                                    qt_methods,
                                                    qt_properties,
                                                    qt_enums,
                                                    qt_constructors,
                                                    qt_classinfo);
  }
};
}
