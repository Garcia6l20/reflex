#pragma once

#include <reflex/meta.hpp>

namespace reflex::meta
{
namespace registry_detail
{
template <class scope, size_t I> struct registered_type_tag
{
  consteval friend auto resolve(registered_type_tag<scope, I>);
};

template <class scope, size_t I>
constexpr meta::info registered_type = [] consteval { return resolve(registered_type_tag<scope, I>()); }();

template <class scope, size_t I, auto = [] {}> consteval bool is_resolved()
{
  if constexpr(requires {
                 { resolve(registered_type_tag<scope, I>()) } -> std::same_as<meta::info>;
               })
  {
    return true;
  }
  else
  {
    return false;
  }
}

template <class scope, auto defer = [] {}> consteval size_t count()
{
  static constexpr auto max_types = 512uz;
  template for(constexpr auto ii : std::views::iota(0uz, max_types)) // iota's unreachable sentinel dont work here
  {
    if constexpr(is_resolved<scope, ii, defer>())
    {
      // pass
    }
    else
    {
      return ii;
    }
  }
  std::unreachable();
}

template <class scope, auto always = [] {}> consteval auto types()
{
  return std::views::iota(0uz, count<scope, always>()) |
         std::views::transform(
             [](auto II)
             {
               return extract<meta::info>(substitute(^^registered_type,
                                                     {
                                                         ^^scope,
                                                         std::meta::reflect_constant(II)}));
             });
}

template <class T, class scope> struct registerer
{
  static constexpr size_t II   = count<scope>();
  static constexpr auto   type = ^^T;
  consteval friend auto   resolve(registered_type_tag<scope, II>)
  {
    return type;
  };
};
} // namespace registry_detail

template <class scope> struct registry
{
  template <typename T> static consteval auto __enroll()
  {
    return registry_detail::registerer<T, scope>{};
  }

  template <typename T> static consteval auto add()
  {
    return __enroll<T, scope>();
  }

  static consteval auto add(meta::info R)
  {
    return reflect_invoke(substitute(^^__enroll,
                                     {
                                         R}),
                          {});
  }

  template <auto defer = [] {}> static consteval auto size()
  {
    return registry_detail::count<scope, defer>();
  }

  template <size_t index, auto defer = [] {}> static consteval auto has_index()
  {
    return registry_detail::is_resolved<scope, index, defer>();
  }

  template <size_t index, auto defer = [] {}> static consteval auto at()
  {
    return registry_detail::types<scope, defer>()[index];
  }

  template <auto defer = [] {}> static consteval auto all()
  {
    return registry_detail::types<scope, defer>();
  }

  template <auto type> static consteval auto contains()
  {
    return std::ranges::contains(all<type>(), decay(type));
  }

  template <typename T> static consteval auto contains()
  {
    return contains<^^T>();
  }

  template <typename T, auto = __enroll<T>()> struct auto_
  {
  };
};
} // namespace reflex::meta
