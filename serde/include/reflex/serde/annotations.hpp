#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/caseconv.hpp>
#include <reflex/concepts.hpp>
#include <reflex/constant.hpp>

#include <cstring>
#include <ranges>
#include <vector>
#endif

#include <reflex/const_check.hpp>

REFLEX_EXPORT namespace reflex::serde
{
  struct rename : constant_string
  {
    consteval rename(std::string_view name) : constant_string{name}
    {
      for(char c : name)
      {
        REFLEX_META_CHECK(
            c != '.' and c != '"' and c != '\\' and static_cast<unsigned char>(c) >= 0x20,
            "a serde::rename cannot contain a dot, a quote, a backslash or a control character",
            ^^rename);
      }
    }
  };

  using naming = caseconv::naming;

  /** @brief Omit a member from serialized output when it carries nothing.
   *
   * A disengaged `std::optional` and an empty range are what carrying nothing
   * means. Opt-in per member, or per type with a filter:
   *
   *     struct [[= serde::omit_if_empty{^^std::optional}]] config
   *     {
   *       std::optional<int>       port;
   *       std::vector<std::string> tags;
   *     };
   *
   * @p types names which member types the annotation reaches. An empty list
   * reaches every type that can be empty. An entry naming a template matches any
   * instance of it, an entry naming a type matches that type.
   *
   * @throws std::meta::exception when an entry names neither a type nor a
   * template.
   */
  struct omit_if_empty
  {
    constant<std::vector<meta::info>> which;

    consteval omit_if_empty() : which{std::vector<meta::info>{}} {}

    consteval omit_if_empty(std::initializer_list<meta::info> types)
        : which{std::vector<meta::info>(types)}
    {
      for(auto entry : types)
      {
        REFLEX_META_CHECK(
            is_type(entry) or is_template(entry),
            "a serde::omit_if_empty entry must name a type or a template",
            ^^omit_if_empty);
      }
    }
  };

  /** @brief Keep a member in the output even where a type-level
   * `serde::omit_if_empty` would omit it, and where a backend rejects omission
   * outright. */
  constexpr struct no_omit_t
  {
  } no_omit;

  namespace detail
  {
    template <typename T>
    concept char_array_c =
        array_of_c<T>
        and std::same_as<std::remove_cvref_t<typename std::remove_cvref_t<T>::value_type>, char>;

    template <typename T>
    concept empty_measurable_c = optional_c<T> or map_c<T> or seq_c<T> or str_c<T>;

    consteval bool can_be_empty(meta::info type)
    {
      if(meta::is_template_instance_of(type, ^^std::array))
      {
        return meta::eval_concept(^^char_array_c, {type});
      }
      return meta::eval_concept(^^empty_measurable_c, {type});
    }

    consteval bool omit_filter_matches(omit_if_empty const& annotation, meta::info type)
    {
      if((*annotation.which).empty())
      {
        return true;
      }
      for(auto entry : *annotation.which)
      {
        if(is_template(entry))
        {
          if(meta::is_template_instance_of(type, entry))
          {
            return true;
          }
        }
        else if(dealias(entry) == type)
        {
          return true;
        }
      }
      return false;
    }
  }

  /** @brief Whether this member is left out of the output when it is empty.
   *
   * @param member_info a nonstatic data member.
   * @return true when a `serde::omit_if_empty` is in effect on @p member_info.
   * @throws std::meta::exception when the annotation cannot mean anything where
   * it sits: a member-level one on a type that can never be empty or excluded by
   * its own filter, or a `serde::no_omit` on a type rather than on a member.
   */
  consteval bool omits_when_empty(meta::info member_info)
  {
    auto parent_info = parent_of(member_info);

    REFLEX_META_CHECK(
        not meta::has_annotation(parent_info, ^^no_omit_t),
        "serde::no_omit belongs on a member, not on a type: nothing reaches a type for it to "
        "cancel",
        ^^no_omit_t);

    if(meta::has_annotation(member_info, ^^no_omit_t))
    {
      return false;
    }

    auto type = dealias(decay(type_of(member_info)));

    if(meta::has_annotation(member_info, ^^omit_if_empty))
    {
      auto annotation = meta::annotation_value_of_with<omit_if_empty>(member_info);
      REFLEX_META_CHECK(
          detail::can_be_empty(type),
          "a serde::omit_if_empty on a type that can never be empty would never fire: only an "
          "optional, a range, a string or a std::array of char can be empty",
          ^^omit_if_empty);
      REFLEX_META_CHECK(
          detail::omit_filter_matches(annotation, type),
          "this serde::omit_if_empty lists no type matching the member it annotates, so it would "
          "never fire: drop the list, or list the member's own type",
          ^^omit_if_empty);
      return true;
    }

    if(meta::has_annotation(parent_info, ^^omit_if_empty))
    {
      auto annotation = meta::annotation_value_of_with<omit_if_empty>(parent_info);
      return detail::can_be_empty(type) and detail::omit_filter_matches(annotation, type);
    }

    return false;
  }

  /** @brief Whether a value carries nothing, as `serde::omit_if_empty` means it.
   *
   * @return true for a disengaged optional, an empty range, an empty string and a
   * `std::array` of char whose first byte is NUL.
   */
  template <typename F> constexpr bool is_empty_value(F const& value)
  {
    using T = std::remove_cvref_t<F>;
    if constexpr(optional_c<T>)
    {
      return not value.has_value();
    }
    else if constexpr(detail::char_array_c<T>)
    {
      return value.empty() or value[0] == '\0';
    }
    else if constexpr(std::is_pointer_v<T>)
    {
      return value == nullptr or *value == '\0';
    }
    else if constexpr(str_c<T>)
    {
      return std::string_view{value}.empty();
    }
    else
    {
      return std::ranges::empty(value);
    }
  }

  consteval constant_string serialized_name(meta::info member_info)
  {
    auto rename_annotations = meta::annotations_of_with(member_info, ^^rename);
    if(!rename_annotations.empty())
    {
      return extract<rename>(rename_annotations.front());
    }

    auto name = identifier_of(member_info);

    if(auto naming_annotation = meta::annotations_of_with(member_info, ^^serde::naming);
       !naming_annotation.empty())
    {
      auto naming = extract<serde::naming>(naming_annotation.front());
      return caseconv::to_case(name, naming);
    }

    auto parent_info = parent_of(member_info);
    if(auto naming_annotation = meta::annotations_of_with(parent_info, ^^serde::naming);
       !naming_annotation.empty())
    {
      auto naming = extract<serde::naming>(naming_annotation.front());
      return caseconv::to_case(name, naming);
    }

    return name;
  }
} // namespace reflex::serde