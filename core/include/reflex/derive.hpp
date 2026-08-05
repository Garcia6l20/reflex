#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
// no includes
#endif

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>

REFLEX_EXPORT namespace reflex
{
  template <typename... Args> struct derive
  {
    consteval derive(Args const&...) {};
  };

  namespace _derive_detail
  {
  consteval bool has_derive_annotation_for(std::meta::info t, std::meta::info tag)
  {
    if(not is_type(tag))
    {
      tag = type_of(tag);
    }
    tag = decay(tag);
    for(const auto& annotation : meta::annotations_of_with(t, ^^derive))
    {
      for(const auto& arg : template_arguments_of(type_of(annotation)))
      {
        if(decay(arg) == tag)
        {
          return true;
        }
      }
    }
    return false;
  }

  consteval bool has_derive_annotation_for(std::meta::info t, auto tag)
  {
    return has_derive_annotation_for(t, ^^decltype(tag));
  }

  // Declared, never defined. mark_as_derived completes one specialization per
  // opted-in (type, tag) pair, so completeness is the out-of-line opt-in flag.
  template <typename T, auto Tag> struct marked_as_derives;
  } // namespace _derive_detail

  template <auto Tag> using derive_t = std::remove_cvref_t<decltype(Tag)>;

  template <typename T, auto Tag>
  constexpr bool derives_v = is_complete_type(substitute(
                                 ^^_derive_detail::marked_as_derives,
                                 {
                                     ^^T, std::meta::reflect_constant(Tag)}))
                          or _derive_detail::has_derive_annotation_for(^^T, Tag);

  consteval bool derives(std::meta::info t, auto tag)
  {
    return std::meta::extract<bool>(substitute(
        ^^derives_v, {
                         t, std::meta::reflect_constant(tag)}));
  }

  template <typename T, typename Tag>
  concept derives_c = derives_v<T, Tag{}>;

  // Opts a type that cannot carry a derive annotation (a third-party one) into
  // the given tags. Must appear before anything queries derives_v for the same
  // pair: the query instantiates derives_v, and its value is cached from then
  // on, so a later mark is silently ignored.
  template <typename T, auto... Tags> consteval void mark_derives()
  {
    (define_aggregate(
         substitute(
             ^^_derive_detail::marked_as_derives,
             {
                 ^^T, std::meta::reflect_constant(Tags)}),
         {}),
     ...);
  }
}