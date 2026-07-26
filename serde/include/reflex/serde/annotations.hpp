#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/caseconv.hpp>
#include <reflex/constant.hpp>
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