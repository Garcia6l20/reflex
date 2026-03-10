export module reflex.serde;

export import reflex.core;

import std;

export namespace reflex::serde
{
struct rename : constant_string
{
};

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

struct aggregate_member
{
  meta::info      info;
  constant_string name;
};

// inline consteval auto
//     members_of(meta::info                aggregate_info,
//                std::meta::access_context context = std::meta::access_context::current())
// {
//   if(not is_aggregate_type(aggregate_info))
//   {
//     throw std::runtime_error("serde::members_of can only be used with aggregate types");
//   }
//   // GCC bug ??
//   // return std::meta::nonstatic_data_members_of(aggregate_info, context)
//   //      | std::views::transform([](auto i) { return aggregate_member{i, serialized_name(i)}; });

//   std::vector<aggregate_member> result;
//   for(auto m : nonstatic_data_members_of(aggregate_info, context))
//   {
//     result.push_back(aggregate_member{m, constant_string(serialized_name(m))});
//   }
//   return result;
// }
} // namespace reflex::serde
