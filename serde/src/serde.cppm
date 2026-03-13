export module reflex.serde;

export import reflex.core;

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

template <typename T> struct object_visitor;

template <aggregate_c T> struct object_visitor<T>
{
  static constexpr auto __access_context = std::meta::access_context::current();

  template <typename Fn, typename StrT, decays_to_c<T> Agg>
  static inline constexpr decltype(auto) operator()(Fn&& fn, StrT&& key, Agg&& agg)
  {
    template for(constexpr auto& member : define_static_array(
                     nonstatic_data_members_of(remove_reference(^^T), __access_context)))
    {
      if(serialized_name(member) == key)
      {
        return std::forward<Fn>(fn)(std::forward<Agg>(agg).[:member:]);
      }
    }
    throw std::runtime_error(
        std::format("Key '{}' does not match any member of {}", key, identifier_of(decay(^^T))));
  }
};

template <typename T>
concept object_visitable_c = is_complete_type(^^object_visitor<std::decay_t<T>>);

template <object_visitable_c T, typename Fn>
constexpr decltype(auto) visit_object(Fn&& fn, auto&& key, T&& value)
{
  return object_visitor<std::decay_t<T>>{}(
      std::forward<Fn>(fn), std::forward<decltype(key)>(key), std::forward<T>(value));
}

} // namespace reflex::serde
