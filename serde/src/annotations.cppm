export module reflex.serde:annotations;

import reflex.core;
import std;

export namespace reflex::serde
{
struct rename : constant_string
{};

using naming = caseconv::naming;

consteval std::string_view serialized_name(meta::info member_info)
{
  auto rename_annotations = annotations_of_with_type(member_info, ^^rename);
  if(!rename_annotations.empty())
  {
    return extract<rename>(rename_annotations.front());
  }

  auto name = identifier_of(member_info);

  if(auto naming_annotation = annotations_of_with_type(member_info, ^^serde::naming);
     !naming_annotation.empty())
  {
    auto naming = extract<serde::naming>(naming_annotation.front());
    return constant_string(caseconv::to_case(name, naming));
  }

  auto parent_info = parent_of(member_info);
  if(auto naming_annotation = annotations_of_with_type(parent_info, ^^serde::naming);
     !naming_annotation.empty())
  {
    auto naming = extract<serde::naming>(naming_annotation.front());
    return constant_string(caseconv::to_case(name, naming));
  }

  return name;
}
} // namespace reflex::serde