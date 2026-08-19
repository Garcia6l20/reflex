#pragma once

#include <reflex/meta.hpp>
#include <reflex/qt/access.hpp>
#include <reflex/qt/detail/gadget_impl.hpp>
#include <reflex/qt/detail/meta_strings.hpp>
#include <reflex/qt/detail/metatype.hpp>
#include <reflex/qt/detail/property.hpp>
#include <reflex/qt/moc/module.hpp>
#include <reflex/serde/json.hpp>

#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

namespace reflex::qt::moc
{
/** @brief the schema revision moc stamps on a metatypes document
 *
 * Not `QtMocConstants::OutputRevision`, which is the `QMetaObject` data revision
 * and reads 13 on Qt 6.11.1, and not published by any Qt header. Measured from
 * `moc --output-json` on the pinned Qt; the guard in
 * `reflex/qt/detail/version.hpp` is what keeps it honest. `qmltyperegistrar`
 * 6.11.1 does not read it.
 */
inline constexpr int output_revision = 69;

struct named_value
{
  std::string name;
  std::string value;
};

struct meta_argument
{
  std::optional<std::string> name;
  std::string                type;
};

struct method_meta
{
  std::string                access;
  std::vector<meta_argument> arguments;
  int                        index = 0;
  std::optional<bool>        isCloned;
  std::optional<bool>        isConst;
  int                        lineNumber = 0;
  std::string                name;
  std::string                returnType;
};

struct property_meta
{
  bool                                 constant   = false;
  bool                                 designable = true;
  bool                                 final      = false;
  int                                  index      = 0;
  int                                  lineNumber = 0;
  std::optional<std::string>           member;
  std::string                          name;
  std::optional<std::string>           notify;
  [[= serde::rename{"override"}]] bool override_ = false;
  std::optional<std::string>           read;
  bool                                 required   = false;
  bool                                 scriptable = true;
  bool                                 stored     = true;
  std::string                          type;
  bool                                 user = false;
  [[= serde::rename{"virtual"}]] bool  virtual_ = false;
  std::optional<std::string>           write;
};

struct superclass_meta
{
  std::string access;
  std::string name;
};

struct enums_meta
{
  std::optional<std::string> alias;
  bool                       isClass    = false;
  bool                       isFlag     = false;
  int                        lineNumber = 0;
  std::string                name;
  std::vector<std::string>   values;
};

struct class_meta
{
  std::vector<named_value>   classInfos;
  std::string                className;
  std::vector<enums_meta>    enums;
  std::optional<bool>        gadget;
  int                        lineNumber = 0;
  std::vector<method_meta>   methods;
  std::optional<bool>        object;
  std::vector<property_meta> properties;
  std::string                qualifiedClassName;
  [[= serde::rename{"signals"}]] std::vector<method_meta> signal_methods;
  [[= serde::rename{"slots"}]] std::vector<method_meta>   slot_methods;
  std::vector<superclass_meta>                            superClasses;
};

struct filemeta_data
{
  std::vector<class_meta> classes;
  std::string             inputFile;
  int                     outputRevision = output_revision;
};

namespace detail
{
constexpr bool omitted(auto const&)
{
  return false;
}

template <typename T> constexpr bool omitted(std::optional<T> const& value)
{
  return not value.has_value();
}

template <typename T> constexpr bool omitted(std::vector<T> const& value)
{
  return value.empty();
}

consteval auto access_name_of(meta::info R) -> std::string_view
{
  if(meta::is_public(R))
  {
    return "public";
  }
  return meta::is_protected(R) ? "protected" : "private";
}

/** @brief the type name to write for @p R, as read from @p T's own scope
 *
 * A property or parameter typed with a `QFlags` alias is spelled by the alias,
 * the way moc spells what the user wrote, rather than by the `QFlags`
 * specialization it stands for. Every other name is qualified, which is what
 * `qmltyperegistrar` needs to resolve an enumeration nested in a namespace.
 */
template <typename T> consteval auto type_name_of(meta::info R) -> std::string
{
  const auto normalized = qt::detail::normalized_type_of(R);
  for(auto const& e : qt::detail::gadget_impl<T>::strings::enums)
  {
    if(meta::dealias(e.type) == normalized)
    {
      return std::string{display_string_of(^^T)} + "::" + std::string{identifier_of(e.type)};
    }
  }
  return qt::detail::normalized_type_name(normalized);
}
} // namespace detail

/** @brief the metatypes description of the reflex.qt class @p T
 *
 * Filled from the same reflections `meta_strings` builds the `QMetaObject` from,
 * so the document and the blob cannot drift apart. Nothing is read back out of a
 * built `QMetaObject`: the readback loses parameter names and accessor names,
 * which are exactly what QML tooling shows.
 */
template <typename T> auto describe() -> class_meta
{
  using strings = typename qt::detail::gadget_impl<T>::strings;

  class_meta described;
  described.className          = std::string{identifier_of(^^T)};
  described.qualifiedClassName = std::string{display_string_of(^^T)};
  described.lineNumber         = int(source_location_of(^^T).line());

  if constexpr(strings::is_object)
  {
    described.object = true;
    described.superClasses.push_back(
        {"public", std::string{display_string_of(meta::dealias(^^typename T::parent_type))}});
  }
  else
  {
    described.gadget = true;
  }

  for(std::size_t i = 0; i + 1 < strings::classinfo_strings.size(); i += 2)
  {
    described.classInfos.push_back({std::string{*strings::classinfo_strings[i]},
                                    std::string{*strings::classinfo_strings[i + 1]}});
  }

  int property_index = 0;
  template for(constexpr auto p : strings::properties)
  {
    constexpr auto spec      = qt::detail::property_spec_of(p);
    constexpr auto reader    = qt::detail::accessor_for<^^qt::getter_t>(^^T, p);
    constexpr auto writer    = qt::detail::accessor_for<^^qt::setter_t>(^^T, p);
    constexpr auto type_text = std::define_static_string(detail::type_name_of<T>(type_of(p)));

    property_meta described_property;
    described_property.constant   = spec.constant;
    described_property.final      = spec.final;
    described_property.index      = property_index++;
    described_property.lineNumber = int(source_location_of(p).line());
    described_property.name       = std::string{identifier_of(p)};
    described_property.required   = spec.required;
    described_property.type       = std::string{type_text};

    if constexpr(spec.read and reader != meta::null)
    {
      described_property.read = std::string{identifier_of(reader)};
    }
    if constexpr(spec.writable())
    {
      if constexpr(writer != meta::null)
      {
        described_property.write = std::string{identifier_of(writer)};
      }
      else
      {
        described_property.member = std::string{identifier_of(p)};
      }
    }
    if constexpr(strings::is_object and spec.notifying())
    {
      constexpr auto notify_text = std::define_static_string(qt::detail::notifier_name_of(p));
      described_property.notify  = std::string{notify_text};
    }
    described.properties.push_back(std::move(described_property));
  }

  template for(constexpr auto e : strings::enums)
  {
    constexpr bool scoped = meta::is_scoped_enum_type(e.keys);
    constexpr bool flag   = e.type != e.keys;

    enums_meta described_enum;
    described_enum.isClass    = scoped;
    described_enum.isFlag     = flag;
    described_enum.lineNumber = int(source_location_of(e.type).line());
    described_enum.name       = std::string{identifier_of(e.type)};
    if constexpr(flag)
    {
      described_enum.alias = std::string{identifier_of(e.keys)};
    }
    template for(constexpr auto k : std::define_static_array(meta::enumerators_of(e.keys)))
    {
      described_enum.values.push_back(std::string{identifier_of(k)});
    }
    described.enums.push_back(std::move(described_enum));
  }

  int method_index = 0;
  template for(constexpr auto entry : strings::methods)
  {
    method_meta described_method;
    described_method.index = method_index++;

    if constexpr(entry.kind == qt::detail::method_kind::notifier)
    {
      constexpr auto notify_text =
          std::define_static_string(qt::detail::notifier_name_of(entry.member));

      described_method.access     = "public";
      described_method.lineNumber = int(source_location_of(entry.member).line());
      described_method.name       = std::string{notify_text};
      described_method.returnType = "void";
    }
    else
    {
      constexpr auto fn          = qt::detail::call_function_of(entry.member);
      constexpr auto return_text = std::define_static_string(
          detail::type_name_of<T>(qt::detail::call_return_type_of(entry.member)));

      described_method.access     = std::string{detail::access_name_of(entry.member)};
      described_method.lineNumber = int(source_location_of(entry.member).line());
      described_method.name       = std::string{identifier_of(entry.member)};
      described_method.returnType = std::string{return_text};

      if constexpr(entry.cloned)
      {
        described_method.isCloned = true;
      }
      if constexpr(meta::is_function(entry.member) and meta::is_const(entry.member))
      {
        described_method.isConst = true;
      }

      template for(constexpr auto argument :
                   std::define_static_array(parameters_of(fn) | std::views::take(entry.arity)
                                            | std::ranges::to<std::vector<meta::info>>()))
      {
        constexpr auto argument_text =
            std::define_static_string(detail::type_name_of<T>(type_of(argument)));

        meta_argument described_argument;
        described_argument.type = std::string{argument_text};
        if constexpr(meta::has_identifier(argument))
        {
          described_argument.name = std::string{identifier_of(argument)};
        }
        described_method.arguments.push_back(std::move(described_argument));
      }
    }

    if constexpr(entry.kind == qt::detail::method_kind::slot)
    {
      described.slot_methods.push_back(std::move(described_method));
    }
    else if constexpr(entry.kind == qt::detail::method_kind::invocable)
    {
      described.methods.push_back(std::move(described_method));
    }
    else
    {
      described.signal_methods.push_back(std::move(described_method));
    }
  }

  return described;
}

/** @brief serializes a metatypes aggregate the way moc writes one
 *
 * An absent field is left out rather than written as `null` or `[]`: moc omits
 * it, and `qmltyperegistrar` reads presence, so a `member` on a read-only
 * property would make it writable in the generated `.qmltypes`. serde's own
 * aggregate serializer writes every member, which is why this takes over for the
 * schema types.
 */
template <typename OutputIt, typename Agg>
  requires(meta::is_class_type(^^Agg) and std::is_aggregate_v<Agg>
           and meta::parent_of(^^Agg) == ^^reflex::qt::moc)
auto tag_invoke(tag_t<serde::serialize>, serde::json::serializer<OutputIt>& ser, Agg const& value)
    -> OutputIt
{
  ser.write_char('{');
  bool first = true;
  template for(constexpr auto member : std::define_static_array(
                   meta::nonstatic_data_members_of(^^Agg, meta::access_context::current())))
  {
    auto const& field = value.[:member:];
    if(not detail::omitted(field))
    {
      if(not first)
      {
        ser.write_char(',');
      }
      first = false;
      ser.write_raw(serde::json::detail::quoted_key<member>());
      serde::serialize(ser, field);
    }
  }
  ser.write_char('}');
  return ser.out();
}
} // namespace reflex::qt::moc
