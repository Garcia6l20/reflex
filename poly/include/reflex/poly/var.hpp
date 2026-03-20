#pragma once

#include <reflex/core.hpp>

#include <map>
#include <vector>
#include <string>
#include <variant>

namespace reflex::poly
{
struct null_t
{
  constexpr bool operator==(null_t const&) const noexcept
  {
    return true;
  }
  constexpr bool operator==(auto const&) const noexcept
  {
    return false;
  }
};

constexpr null_t null{};

using string = std::string;

template <typename... Ts> struct var;
template <typename... Ts> struct obj;
template <typename... Ts> struct arr;

template <typename... Ts> struct obj : std::map<string, var<Ts...>>
{
  using std::map<string, var<Ts...>>::map;
};

template <typename... Ts> struct arr : std::vector<var<Ts...>>
{
  using std::vector<var<Ts...>>::vector;
};

namespace detail
{

template <typename T> struct wrap_reference
{
  using type = T;
};

template <typename T> struct wrap_reference<T&>
{
  using type = std::remove_reference_t<T>*;
};

template <typename T> struct wrap_reference<T const&>
{
  using type = std::remove_reference_t<T const>*;
};

template <typename... Ts>
using base_variant_type = std::variant<
    null_t,
    typename wrap_reference<Ts>::type...,
    arr<Ts...>,
    obj<Ts...>,
    var<Ts...>*,
    arr<Ts...>*,
    obj<Ts...>*>;
} // namespace detail

template <typename... Ts> struct var : detail::base_variant_type<Ts...>
{
  using arr_type = arr<Ts...>;
  using obj_type = obj<Ts...>;

  using variant_type = detail::base_variant_type<Ts...>;

  struct infos
  {
    static constexpr auto base_types = std::array{^^null_t, ^^Ts...};

    static consteval std::meta::info __integral_type()
    {
      for(auto t : base_types)
      {
        if(is_int_number_type(t))
        {
          return t;
        }
      }
      return ^^void;
    }

    static consteval std::meta::info __floating_point_type()
    {
      for(auto t : base_types)
      {
        if(is_floating_point_type(t))
        {
          return t;
        }
      }
      return ^^void;
    }
  };

  using integral_type                     = typename[:infos::__integral_type():];
  static constexpr bool has_integral_type = dealias(^^integral_type) != ^^void;

  using floating_point_type                     = typename[:infos::__floating_point_type():];
  static constexpr bool has_floating_point_type = dealias(^^floating_point_type) != ^^void;

  using variant_type::variant_type; // inherit constructors

  template <typename T>
  constexpr var(std::reference_wrapper<T> ref) : variant_type(std::addressof(ref.get()))
  {}

  // === numeric catch-all (int, float, size_t, …)
  //   constexpr var(detail::number_c auto n)
  //     requires(not std::constructible_from<detail::base_value, decltype(n)>)
  //       : variant_type(number(n))
  //   {}

  // === conversion constructors
  template <typename Int>
    requires(
        is_int_number_type(^^Int)
        and (not std::constructible_from<variant_type, Int &&>)
        and has_integral_type
        and std::convertible_to<Int, integral_type>)
  constexpr var(Int&& value) : variant_type(integral_type(value))
  {}

  template <typename Int>
    requires(
        is_int_number_type(^^Int)
        and (not std::constructible_from<variant_type, Int &&>)
        and (not has_integral_type)
        and has_floating_point_type
        and std::convertible_to<Int, floating_point_type>)
  constexpr var(Int&& value) : variant_type(floating_point_type(value))
  {}

// #define REFLEX_VAR_DEBUG_COPIES
#ifdef REFLEX_VAR_DEBUG_COPIES
  var(var const& other) : variant_type(other)
  {
    std::println("var copy constructor");
  }
#endif

  // === initializer-list construction  {"key": val, ...}
  constexpr var(std::initializer_list<std::pair<string const, var<Ts...>>> pairs)
      : variant_type(obj_type(pairs))
  {}

  // === type predicates
  template <typename T> constexpr bool is() const
  {
    if constexpr(is_reference_type(^^T))
    {
      return std::holds_alternative<std::remove_reference_t<T>*>(*this);
    }
    else
    {
      return std::holds_alternative<T>(*this);
    }
  }

  constexpr bool is_array() const
  {
    return std::holds_alternative<arr_type>(*this);
  }

  constexpr bool is_object() const
  {
    return std::holds_alternative<obj_type>(*this);
  }

  constexpr bool is_null() const
  {
    return std::holds_alternative<null_t>(*this);
  }

  // === typed accessors
  template <typename T> constexpr decltype(auto) as(this auto&& self)
  {
    if constexpr(is_reference_type(^^T))
    {
      return *std::get<std::remove_reference_t<T>*>(self);
    }
    else
    {
      return std::get<T>(self);
    }
  }

  constexpr decltype(auto) as_object(this auto&& self)
  {
    return std::get<obj_type>(self);
  }

  constexpr decltype(auto) as_array(this auto&& self)
  {
    return std::get<arr_type>(self);
  }

  // === get<T>() with optional fallback
  template <typename T, typename Self>
  constexpr std::optional<const_like_t<Self, T>&> get(this Self& self) noexcept
  {
    if constexpr(is_reference_type(^^T))
    {
      if(auto* p = std::get_if<std::remove_reference_t<T>*>(&self))
        return **p;
    }
    else
    {
      if(auto* p = std::get_if<T>(&self))
        return *p;
    }
    return std::nullopt;
  }

  // === array index access
  constexpr decltype(auto) operator[](this auto&& self, std::size_t idx)
  {
    return self.template as<arr_type>()[idx];
  }

  // == object key access
  constexpr var& operator[](std::string_view key)
  {
    auto val = at_path(key);
    if(val == nullptr)
    {
      throw std::runtime_error("insert failed");
    }
    return *val;
  }

  constexpr std::optional<var const&> at(std::string_view key) const noexcept
  {
    auto val = at_path(key);
    if(val == nullptr)
    {
      return std::nullopt;
    }
    return *val;
  }

private:
  //    Returns a pointer to the nested value, or nullptr if any step is missing.
  constexpr auto at_path(this auto& self, std::string_view path) noexcept
      -> const_like_t<decltype(self), var>*
  {
    auto cur = &self;
    while(!path.empty())
    {
      if(!cur->is_object())
      {
        if constexpr(requires { *cur = obj_type{}; })
        {
          *cur = obj_type{};
        }
        else
        {
          return nullptr;
        }
      }
      const auto     dot = path.find('.');
      const auto     key = path.substr(0, dot);
      decltype(auto) obj = cur->template as<obj_type>();
      auto it = std::ranges::find_if(obj, [key](auto const& elem) { return elem.first == key; });
      if(it == obj.end())
      {
        if constexpr(requires { obj.emplace(key, null); })
        {
          it = obj.emplace(key, null).first;
        }
        else
        {
          return nullptr;
        }
      }
      cur  = &it->second;
      path = (dot == std::string_view::npos) ? std::string_view{} : path.substr(dot + 1);
    }
    return cur;
  }

public:
  // === contains  (single key or dotted path)
  constexpr bool contains(std::string_view path) const noexcept
  {
    return at_path(path) != nullptr;
  }

  // === size / empty ===
  constexpr std::size_t size() const
  {
    return reflex::visit(
        match{
            [](null_t const&) -> std::size_t { return 0; },
            [](auto const& s) -> std::size_t
              requires requires { s.size(); }
            { return s.size(); },
            [](auto const&) -> std::size_t { return 1; },
            },
            *this);
  }

  constexpr bool empty() const
  {
    return size() == 0;
  }

  // === push_back for arrays
  constexpr void push_back(auto&& v)
  {
    as<arr_type>().push_back(std::forward<decltype(v)>(v));
  }

  // === merge another object into this one
  constexpr void merge(obj_type const& other)
  {
    auto& obj = as<obj_type>();
    for(auto const& [k, v] : other)
    {
      obj.insert_or_assign(k, v);
    }
  }

  // === visit helper
  constexpr decltype(auto) visit(this auto&& self, auto&& fn)
  {
    return reflex::visit(std::forward<decltype(fn)>(fn), std::forward<decltype(self)>(self));
  }

  // === equality operators

  template <typename T> constexpr bool operator==(T const& other) const
  {
    return reflex::visit(
        match{
            [&other](auto const& s) -> bool
              requires requires { s == other; }
            { return s == other; },
            [&other](auto const&) //
            { return false; },
            },
            *this);
  }
};

} // namespace reflex::poly

namespace reflex
{

#define fwd(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

/** @brief variant visitor */
template <typename... Ts> struct visitor<poly::var<Ts...>>
{
  template <typename Fn, typename VarT>
  static inline constexpr decltype(auto)
      operator()(Fn&& fn, VarT&& v) noexcept(noexcept(fwd(fn)(std::get<0>(fwd(v)))))
  {
    static constexpr auto value_types =
        define_static_array(template_arguments_of(variant_type_of(^^poly::var<Ts...>)));
    template for(constexpr auto ii : std::views::iota(0uz, value_types.size()))
    {
      constexpr auto type = value_types[ii];
      if(v.index() == ii)
      {
        if constexpr(is_pointer_type(type))
        {
          if constexpr(decay(remove_pointer(type)) == ^^poly::var<Ts...>)
          {
            return visitor<poly::var<Ts...>>{}(std::forward<Fn>(fn), *std::get<ii>(fwd(v)));
          }
          else
          {
            return fwd(fn)(*std::get<ii>(fwd(v)));
          }
        }
        else
        {
          return fwd(fn)(std::get<ii>(fwd(v)));
        }
      }
    }
    std::unreachable();
  }
};
} // namespace reflex

namespace std
{
using namespace reflex::poly;

template <> struct formatter<null_t> : formatter<std::string_view>
{
  auto format(null_t const&, auto& ctx) const
  {
    return std::formatter<std::string_view>::format("null", ctx);
  }
};

template <typename... Ts> struct formatter<var<Ts...>> : formatter<std::string_view>
{
  auto format(var<Ts...> const& v, auto& ctx) const
  {
    using var_t = var<Ts...>;

    v.visit(
        reflex::match{
            [&ctx]<typename T>(T const& s)
              requires((not reflex::decays_to_c<T, typename var_t::arr_type>)
                       and (not reflex::decays_to_c<T, typename var_t::obj_type>)
                       and std::formattable<std::decay_t<T>, char>)
            { std::format_to(ctx.out(), "{}", s); },
            [&ctx](var_t::arr_type const& arr) {
              ctx.out() = '[';
              for(std::size_t i = 0; i < arr.size(); ++i)
              {
                if(i > 0)
                  ctx.out() = ',';
                std::format_to(ctx.out(), "{}", arr[i]);
              }
              ctx.out() = ']';
            },
            [&ctx](var_t::obj_type const& obj) {
              ctx.out()  = '{';
              bool first = true;
              for(auto const& [k, v] : obj)
              {
                if(!first)
                  ctx.out() = ',';
                first = false;
                std::format_to(ctx.out(), "{}:{}", k, v);
              }
              ctx.out() = '}';
            },
            [&ctx]<typename T>(T const&) {
              std::format_to(ctx.out(), "<unformattable:{}>", display_string_of(dealias(^^T)));
            },
        });

    return ctx.out();
  }
};

} // namespace std
