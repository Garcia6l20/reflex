#pragma once

#include <reflex/constant.hpp>
#include <reflex/fwd.hpp>
#include <reflex/meta.hpp>

namespace reflex
{
template <typename... Ts> consteval auto make_index_sequence_for_if(auto matcher)
{
  constexpr auto type_array = std::array{^^Ts...};
  constexpr auto sequence_type =
      substitute(^^std::index_sequence,
                 std::views::iota(0uz, type_array.size())                                      //
                     | std::views::filter([&](auto I) { return matcher(type_array[I]); })      //
                     | std::views::transform([](auto I) { return meta::reflect_constant(I); }) //
                     | std::ranges::to<std::vector>());
  return typename[:sequence_type:]();
}

template <auto matcher> constexpr auto extract_args_if(auto&&... args)
{
  return [&]<auto... I>(std::index_sequence<I...>) {//
    return std::forward_as_tuple(reflex_fwd(args...[I])...);
  }(make_index_sequence_for_if<decltype(args)...>(matcher));
}

template <typename Impl> struct pack;

inline constexpr decltype(auto) forward_as_pack(auto&&... args)
{
  struct impl;
  consteval
  {
    define_aggregate(^^impl,
                     {
                         data_member_spec(^^decltype(args),
                                          {
                                          })...});
  };

  return pack<impl>{{reflex_fwd(args)...}};
}

template <typename Impl> struct pack : Impl
{
  static constexpr auto members =
      define_static_array(meta::nonstatic_data_members_of(^^Impl, meta::access_context::current()));
  static constexpr auto types   = define_static_array(members | std::views::transform(meta::type_of));
  static constexpr auto size    = members.size();
  static constexpr auto indexes = std::views::iota(0uz, size);

  template <size_t I>
    requires(I < size)
  constexpr decltype(auto) get(this auto&& self) noexcept
  {
    return reflex_fwd(self.[:members[I]:]);
  }

  template <size_t... I>
    requires((I < size) and ...)
  constexpr decltype(auto) get(this auto&& self, std::index_sequence<I...>) noexcept
  {
    return std::forward_as_tuple(reflex_fwd(self.[:members[I]:])...);
  }

  template <size_t I>
    requires(I < size)
  constexpr decltype(auto) operator[](this auto&& self, std::integral_constant<size_t, I>) noexcept
  {
    return reflex_fwd(self.[:members[I]:]);
  }

  template <auto matcher> inline constexpr decltype(auto) make_selector(this auto&& self)
  {
    return typename[:substitute(^^std::index_sequence,
                                indexes                                                             //
                                    | std::views::filter([&](auto I) { return matcher(types[I]); }) //
                                    | std::views::transform([](auto I) { return meta::reflect_constant(I); })):]();
  }

  template <auto matcher> inline constexpr decltype(auto) get_if(this auto&& self)
  {
    constexpr auto selector = self.template make_selector<matcher>();
    return self.get(selector);
  }

  template <auto... matchers> inline constexpr decltype(auto) select_indexes(this auto&& self)
  {
    constexpr auto selectors = std::tuple(self.template make_selector<matchers>()...);

    constexpr auto remaining_type = [&] consteval
    {
      const auto contains_index = []<std::size_t... Is>(std::index_sequence<Is...>, std::size_t I)
      { return ((I == Is) or ...); };
      const auto selectors_contains_index = [&](std::size_t I)
      {
        template for(constexpr auto selector : selectors)
        {
          if(contains_index(selector, I))
          {
            return true;
          }
        }
        return false;
      };
      return substitute(^^std::index_sequence,
                        indexes                                                                           //
                            | std::views::filter([&](auto I) { return not selectors_contains_index(I); }) //
                            | std::views::transform([](auto I) { return meta::reflect_constant(I); }));
    }();
    constexpr typename[:remaining_type:] remaining;

    return std::tuple_cat(selectors, std::tuple(remaining));
  }

  template <auto... matchers> inline constexpr decltype(auto) select(this auto&& self)
  {
    constexpr auto selectors = self.template select_indexes<matchers...>();
    return [&]<size_t... I>(std::index_sequence<I...>)
    {
      return std::tuple([&]<size_t... J>(std::index_sequence<J...>)
                        { return forward_as_pack(reflex_fwd(self.[:members[J]:])...); }(std::get<I>(selectors))...);
    }(std::make_index_sequence<std::tuple_size_v<decltype(selectors)>>());
  }
};

#define reflex_pack(__args__) forward_as_pack(reflex_fwd(__args__)...)

constexpr auto is_pack_type = meta::make_template_tester(^^pack);

template <size_t I, typename P>
  requires(is_pack_type(^^P))
inline constexpr decltype(auto) get(P&& p) noexcept
{
  return p.template get<I>();
}

template <auto... matchers, typename P>
  requires(is_pack_type(^^P))
inline constexpr decltype(auto) select(P&& p) noexcept
{
  return p.template select<matchers...>();
}

template <auto matcher, typename P>
  requires(is_pack_type(^^P))
inline constexpr decltype(auto) get_if(P&& p) noexcept
{
  return p.template get_if<matcher>();
}

template <typename T> struct kwargs : T
{
};

namespace kwargs_detail
{
constexpr std::string_view parse_name(std::string_view raw)
{
  auto start = raw.find_first_not_of(" =");
  if(start == std::string_view::npos)
  {
    start = 0;
  }
  auto end = raw.find_first_of(" =", start);
  if(end == std::string_view::npos)
  {
    end = raw.size();
  }
  return raw.substr(start, end - start);
}

template <constant_string raw_names> auto make(auto... values)
{
  struct impl;
  consteval
  {
    auto         names    = raw_names.get() | std::views::split(',');
    auto         names_it = std::begin(names);
    meta::vector args;
    template for(constexpr auto ii : std::views::iota(0uz, sizeof...(values)))
    {
      auto       name = parse_name(std::string_view{*names_it});
      meta::info type = ^^decltype(values...[ii]);
      args.push_back(data_member_spec(type, {.name = name}));
      ++names_it;
    }
    define_aggregate(^^impl, args);
  }

  return kwargs<impl>{{reflex_fwd(values)...}};
}
template <constant_string names> auto from_lambda(auto l)
{
  static constexpr auto members =
      define_static_array(nonstatic_data_members_of(^^decltype(l), meta::access_context::unchecked()));
  static constexpr auto n_members = members.size();
  return [&]<size_t... I>(std::index_sequence<I...>)
  { return make<names>(std::move(l).[:members[I]:]...); }(std::make_index_sequence<n_members>());
}
} // namespace kwargs_detail

namespace literals
{

namespace detail
{
template <typename T, char... Chars> consteval auto parse_integral()
{
  constexpr auto data = define_static_array(std::array{Chars...}                                      //
                                            | std::views::drop_while([](auto c) { return c == '0'; }) //
                                            | std::views::transform([](auto c) { return c - '0'; }));
  if constexpr(data.size() == 0)
  {
    return 0;
  }
  else
  {
    T value = data.front();
    if constexpr(data.size() > 1)
    {
      template for(constexpr auto c : data | std::views::drop(1))
      {
        value = (value * 10) + c;
      }
    }
    return value;
  }
}
} // namespace detail

template <char... Chars> constexpr auto operator""_uz()
{
  constexpr auto value = detail::parse_integral<size_t, Chars...>();
  return std::integral_constant<size_t, value>{};
}

template <char... Chars> constexpr auto operator""_sz()
{
  constexpr auto value = detail::parse_integral<std::ptrdiff_t, Chars...>();
  return std::integral_constant<std::ptrdiff_t, value>{};
}

// add unary minus operator to handle negative values (by default the are converted to value_type)
template <std::ptrdiff_t V> constexpr auto operator-(std::integral_constant<std::ptrdiff_t, V>)
{
  return std::integral_constant<std::ptrdiff_t, -V>{};
}
} // namespace literals

} // namespace reflex

#define reflex_kwargs(...) ::reflex::kwargs_detail::from_lambda<#__VA_ARGS__>([__VA_ARGS__] {})
