#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/caseconv.hpp>
#include <reflex/constant.hpp>
#endif

REFLEX_EXPORT namespace reflex::serde
{
  struct rename : constant_string
  {};

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