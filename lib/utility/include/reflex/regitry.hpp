#pragma once

#include <reflex/meta.hpp>
#include <reflex/constant.hpp>
#include <typeindex>

namespace reflex::meta
{
namespace registry_detail
{
struct not_found
{
};

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
  static constexpr size_t npos = size_t(-1);

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

  template <auto defer = [] {}> static constexpr auto at(size_t index)
  {
    return registry_detail::types<scope, defer>()[index];
  }

  template <meta::info type, auto defer = [] {}> static consteval auto index_of()
  {
    auto range = registry_detail::types<scope, defer>();
    auto it    = std::ranges::find(range, type);
    return it == std::ranges::end(range) ? npos : std::ranges::distance(std::ranges::begin(range), it);
  }

  template <typename Fn, auto defer = ^^Fn> static constexpr auto visit(size_t index, Fn const& fn)
  {
    template for(constexpr size_t ii : std::views::iota(0uz, size<defer>()))
    {
      if(ii == index)
      {
        using T = typename[:at<ii, defer>():];
        return fn(std::type_identity<T>{});
      }
    }
    return fn(std::type_identity<registry_detail::not_found>{});
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

  struct runtime_t
  {
    struct entry
    {
      std::type_index  index;
      std::string_view name;
    };
    const std::vector<entry> entries;
  };
  template <auto defer = [] {}> static inline constexpr runtime_t lock() noexcept
  {
    std::vector<typename runtime_t::entry> entries;
    template for(constexpr auto t : all<defer>())
    {
      using T = [:t:];
      entries.push_back(typename runtime_t::entry{typeid(T), display_string_of(t)});
    }
    return runtime_t{
        .entries = entries,
    };
  }
  const static runtime_t runtime;
};

#define REFLEX_LOCK_REGISTRY(__scope) \
  template <> const reflex::meta::registry<__scope>::runtime_t reflex::meta::registry<__scope>::runtime = reflex::meta::registry<__scope>::lock();

} // namespace reflex::meta
