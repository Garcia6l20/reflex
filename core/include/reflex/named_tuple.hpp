#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <string_view>
#endif

#include <reflex/named_arg.hpp>

REFLEX_EXPORT namespace reflex
{
  namespace detail
  {
  template <typename... Ts> struct named_tuple_impl
  {
    struct type;
    consteval
    {
      std::vector<std::meta::info> specs;
      template for(constexpr auto ii : std::views::iota(0uz, sizeof...(Ts)))
      {
        constexpr auto a_type = remove_const(remove_reference(^^typename Ts...[ii] ::type));
        constexpr auto a_name = Ts...[ii] ::name;
        specs.push_back(data_member_spec(a_type, {.name = *a_name}));
      }
      define_aggregate(^^type, specs);
    }
  };

  template <typename... Ts> using named_tuple_impl_t = named_tuple_impl<Ts...>::type;

  } // namespace detail

  template <typename T>
  concept named_tuple_c =
      std::meta::has_parent(decay(^^T))
      and (std::meta::template_of(std::meta::parent_of(decay(^^T))) == ^^detail::named_tuple_impl);

  template <constant_string Name> constexpr decltype(auto) get(named_tuple_c auto&& tuple) noexcept
  {
    static constexpr auto ctx = std::meta::access_context::current();
    template for(constexpr auto member :
                 define_static_array(nonstatic_data_members_of(decay(type_of(^^tuple)), ctx)))
    {
      if constexpr(identifier_of(member) == Name.get())
      {
        return tuple.[:member:];
      }
    }
    std::unreachable();
  }

  template <std::size_t I> constexpr decltype(auto) get(named_tuple_c auto&& tuple) noexcept
  {
    auto&& [... values] = tuple;
    return values...[I];
  }

  template <constant_string... Names, typename... Ts>
  constexpr auto make_named_tuple(named_arg<Names, Ts> && ... args) noexcept
  {
    return detail::named_tuple_impl_t<named_type<Names, Ts>...>{std::forward<Ts>(args.value)...};
  }

  consteval std::meta::info make_named_tuple_from_function_parameters(std::meta::info func)
  {
    std::vector<std::meta::info> specs;
    for(auto p : parameters_of(func))
    {
      auto a_type = remove_cvref(type_of(p));
      auto a_name = identifier_of(p);
      specs.push_back(substitute(
          ^^named_type, {
                            std::meta::reflect_constant(define_static_string(a_name)), a_type}));
    }
    return substitute(^^detail::named_tuple_impl_t, specs);
  }

  template <std::meta::info Fn>
  using named_tuple_from_function_parameters_t = [:make_named_tuple_from_function_parameters(Fn):];

  decltype(auto) apply(auto&& func, named_tuple_c auto&& tuple)
  {
    return std::apply(
        std::forward<decltype(func)>(func), to_tuple(std::forward<decltype(tuple)>(tuple)));
  }
}
