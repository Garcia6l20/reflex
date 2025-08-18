#pragma once

#include <reflex/concepts.hpp>
#include <reflex/detail/portable.hpp>
#include <reflex/fwd.hpp>
#include <reflex/match.hpp>
#include <reflex/meta.hpp>
#include <reflex/none.hpp>
#include <reflex/utility.hpp>
#include <reflex/visit.hpp>

#include <array>
#include <exception>
#include <format>
#include <map>
#include <system_error>
#include <vector>

namespace reflex
{
struct bad_var_access : std::exception
{
  const char* what() const noexcept final
  {
    return "bad var";
  }
};

template <template <class...> class T> struct recursive
{
  static constexpr auto tmpl = ^^T;
};

constexpr auto is_recursive_type = meta::make_template_tester(^^recursive);

namespace _var
{
template <typename... Ts> class var_impl;
}

consteval bool is_var_type(meta::info R)
{
  return meta::is_template_instance_of(dealias(decay(R)), ^^_var::var_impl);
}

namespace _var
{

constexpr auto is_reference_wrapper_type = meta::make_template_tester(^^std::reference_wrapper);

template <size_t, bool, typename...> union variadic_union;

template <size_t index, bool trivially_destructible> union variadic_union<index, trivially_destructible>
{
};

template <size_t index, bool trivially_destructible, typename Head, typename... Tail>
union variadic_union<index, trivially_destructible, Head, Tail...>
{
public:
  REFLEX_FORCE_INLINE constexpr variadic_union() noexcept : dummy_{}
  {
  }

  REFLEX_FORCE_INLINE constexpr variadic_union(std::index_sequence<index>) : head_{}
  {
  }

  template <size_t I>
    requires(I != index)
  REFLEX_FORCE_INLINE constexpr variadic_union(std::index_sequence<I> seq) : tail_{seq}
  {
  }

  template <typename... Args>
  REFLEX_FORCE_INLINE constexpr variadic_union(std::index_sequence<index>, Args&&... args) : head_(reflex_fwd(args)...)
  {
  }

  template <size_t I, typename... Args>
    requires(I != index)
  REFLEX_FORCE_INLINE constexpr variadic_union(std::index_sequence<I> seq, Args&&... args)
      : tail_{seq, reflex_fwd(args)...}
  {
  }

  constexpr ~variadic_union() noexcept
    requires(trivially_destructible)
  = default;

  constexpr ~variadic_union() noexcept
    requires(not trivially_destructible)
  {
  }

  template <typename T>
    requires(std::same_as<T, Head>)
  REFLEX_FORCE_INLINE constexpr decltype(auto) get(this auto&& self) noexcept
  {
    return std::forward_like<decltype(self)>(self.head_);
  }

  template <typename T>
    requires(not std::same_as<T, Head>)
  REFLEX_FORCE_INLINE constexpr decltype(auto) get(this auto&& self) noexcept
  {
    return std::forward_like<decltype(self)>(self.tail_).template get<T>();
  }

  constexpr variadic_union(const variadic_union&)            = default;
  constexpr variadic_union(variadic_union&&)                 = default;
  constexpr variadic_union& operator=(const variadic_union&) = default;
  constexpr variadic_union& operator=(variadic_union&&)      = default;

  char                                                       dummy_;
  Head                                                       head_;
  variadic_union<index + 1, trivially_destructible, Tail...> tail_;
};

template <typename... Ts> class var_impl
{
public:
  using index_type           = [:[]
                      {
                        if constexpr(sizeof...(Ts) < std::numeric_limits<uint8_t>::max())
                        {
                          return ^^uint8_t;
                        }
                        else if constexpr(sizeof...(Ts) < std::numeric_limits<uint16_t>::max())
                        {
                          return ^^uint16_t;
                        }
                        else if constexpr(sizeof...(Ts) < std::numeric_limits<uint32_t>::max())
                        {
                          return ^^uint32_t;
                        }
                        else
                        {
                          static_assert(sizeof...(Ts) < std::numeric_limits<size_t>::max(), "too much types");
                          return ^^size_t;
                        }
                      }():];
  static constexpr auto npos = std::numeric_limits<index_type>::max();

  friend std::formatter<var_impl>;
  friend visitor<var_impl>;

private:
  static constexpr auto ctx_ = meta::access_context::current();

  static constexpr auto types_ = std::array{^^none_t, ^^Ts...};

  static constexpr auto basic_types_ =
      define_static_array(types_                                                                //
                          | std::views::filter([](auto R) { return not is_recursive_type(R); }) //
                          | std::views::transform(
                                [](auto R)
                                {
                                  return is_reference_type(R) ? substitute(^^std::reference_wrapper,
                                                                           {
                                                                               remove_reference(R)})
                                                              : R;
                                }) //
      );

  static constexpr auto recursive_templates_ = define_static_array(
      [] consteval
      {
        meta::vector rt;
        template for(constexpr auto t : types_)
        {
          if constexpr(is_recursive_type(t))
          {
            using RT = [:t:];
            rt.push_back(RT::tmpl);
          }
        }
        return rt;
      }());

  static constexpr auto recursive_types_ = define_static_array(
      [] consteval
      {
        meta::vector rt;
        template for(constexpr auto t : recursive_templates_)
        {
          if constexpr(can_substitute(t, {^^var_impl}))
          {
            rt.push_back(substitute(t, {^^var_impl}));
          }
          else if constexpr(can_substitute(t, {^^std::string, ^^var_impl}))
          {
            rt.push_back(substitute(t, {^^std::string, ^^var_impl}));
          }
          else
          {
            static_assert(always_false<var_impl>, "unhandled recursive type");
          }
        }
        return rt;
      }());

  static constexpr auto alternatives_ =
      define_static_array(std::array{basic_types_, recursive_types_} | std::views::join);

  static constexpr auto vec_type_ = [] consteval
  {
    template for(constexpr auto t : recursive_types_)
    {
      if constexpr(template_of(t) == ^^std::vector)
      {
        return t;
      }
    }
    return ^^void;
  }();

  static constexpr auto map_type_ = [] consteval
  {
    template for(constexpr auto t : recursive_types_)
    {
      if constexpr(template_of(t) == ^^std::map)
      {
        return t;
      }
    }
    return ^^void;
  }();

  static constexpr auto has_vec_ = vec_type_ != ^^void;
  static constexpr auto has_map_ = map_type_ != ^^void;

  static consteval meta::info type_implementation_of(meta::info I)
  {
    if(is_template(I))
    {
      if(has_map_ and (I == template_of(map_type_)))
      {
        return map_type_;
      }
      else if(has_vec_ and (I == template_of(vec_type_)))
      {
        return vec_type_;
      }
      else
      {
        std::unreachable(); // unhandled template type
      }
    }
    else
    {
      return dealias(I);
    }
  }

  static constexpr auto indexes_ = std::views::iota(0uz, alternatives_.size());

  static constexpr auto trivially_destructible_ =
      std::ranges::all_of(alternatives_, [](auto R) { return is_trivially_destructible_type(R); });

  using storage_t = [:[]
                     {
                       meta::vector args = {meta::reflect_constant(size_t(0)),
                                            meta::reflect_constant(trivially_destructible_)};
                       args.append_range(alternatives_);
                       return substitute(^^variadic_union, args);
                     }():];

  storage_t  storage_{};
  index_type current_ = npos;

public:
  template <template <class...> class T>
  using recursive_t = [:[]
                       {
                         constexpr auto R = ^^T;
                         if constexpr(std::ranges::contains(recursive_templates_, R))
                         {
                           if constexpr(can_substitute(R, {^^var_impl}))
                           {
                             return substitute(R, {^^var_impl});
                           }
                           else if(can_substitute(R, {^^var_impl}))
                           {
                             return substitute(R, {^^std::string, ^^var_impl});
                           }
                         }
                         return ^^void;
                       }():];

  using vec_t = [:vec_type_:];
  using map_t = [:map_type_:];

  static constexpr size_t alternative_count() noexcept
  {
    return alternatives_.size();
  }

  static consteval meta::info alternative_at(size_t ii) noexcept
  {
    return alternatives_[ii];
  }

  constexpr void reset() noexcept
  {
    if(current_ == npos)
    {
      return;
    }
    template for(constexpr auto index : indexes_)
    {
      if(index == current_)
      {
        constexpr auto R = alternatives_[index];
        storage_.~storage_t();
        current_ = npos;
        return;
      }
    }
  }

  constexpr bool has_value() const noexcept
  {
    return current_ > 0;
  }

  static consteval bool is_equivalent_to(meta::info I, meta::info R) noexcept
  {
    return ((R == dealias(I)) or (is_reference_wrapper_type(R) and unwrap_reference(R) == dealias(I)) or
            (is_template(I) and meta::is_template_instance_of(R, I)));
  }

  static consteval bool can_hold(meta::info I) noexcept
  {
    for(auto R : alternatives_)
    {
      if(is_equivalent_to(I, R))
      {
        return true;
      }
    }
    if(is_equivalent_to(I, vec_type_))
    {
      return true;
    }
    if(is_equivalent_to(I, map_type_))
    {
      return true;
    }
    return false;
  }

  template <typename T> static constexpr bool can_hold() noexcept
  {
    return can_hold(^^T);
  }

  template <template <typename...> typename T> static constexpr bool can_hold() noexcept
  {
    return can_hold(^^T);
  }

  template <typename... Us>
    requires(sizeof...(Us) > 1)
  static constexpr bool can_hold() noexcept
  {
    return (can_hold<Us>() and ...);
  }

  template <typename T> static constexpr bool constructible_from() noexcept
  {
    return can_hold<std::decay_t<T>>();
  }

  template <typename T> static constexpr bool explicitly_constructible_from() noexcept
  {
    template for(constexpr auto R : alternatives_)
    {
      if constexpr(meta::has_explicit_constructor(R, {^^T}))
      {
        return true;
      }
    }
    return false;
  }

  template <typename T> static constexpr index_type index_of() noexcept
  {
    index_type index = 0;
    template for(constexpr auto R : alternatives_)
    {
      if constexpr(R == ^^T or (is_reference_wrapper_type(R) and unwrap_reference(R) == ^^T))
      {
        return index;
      }
      ++index;
    }
    return index_type(npos);
  }

  template <typename T> constexpr bool has_value() const noexcept
  {
    return current_ == index_of<T>();
  }

  template <template <typename...> typename T> constexpr bool has_value() const noexcept
  {
    return current_ == index_of<typename[:type_implementation_of(^^T):]>();
  }

  constexpr bool has_error() const noexcept
    requires(can_hold<std::error_code>())
  {
    return has_value<std::error_code>();
  }

  constexpr std::error_code error() const noexcept
    requires(can_hold<std::error_code>())
  {
    return get<std::error_code>();
  }

  constexpr var_impl() noexcept
  {
    emplace<none_t>();
  }

  template <typename Fn, typename Self, bool adapt_references = true>
  inline constexpr decltype(auto) visit(this Self&& self, Fn&& fn, std::bool_constant<adapt_references> = {})
  {
    template for(constexpr auto ii : std::views::iota(0uz, self.alternatives_.size()))
    {
      if(ii == self.current_)
      {
        static constexpr auto R = self.alternatives_[ii];
        if constexpr(adapt_references)
        {
          if constexpr(is_reference_wrapper_type(R) and
                       requires { reflex_fwd(fn)(reflex_fwd(self).template get<unwrap_reference(R)>()); })
          {
            return reflex_fwd(fn)(reflex_fwd(self).template get<unwrap_reference(R)>());
          }
          else
          {
            return reflex_fwd(fn)(reflex_fwd(self).template get<R>());
          }
        }
        else if constexpr(requires { reflex_fwd(fn)(reflex_fwd(self).template get<R>()); })
        {
          return reflex_fwd(fn)(reflex_fwd(self).template get<R>());
        }
        else
        {
          static_assert(always_false<Self>, "unvisitable");
        }
      }
    }
    std::unreachable();
  }

  constexpr var_impl(var_impl&& o) noexcept
  {
    std::move(o).visit([this]<typename T>(T&& value) { assign(std::move(value)); }, std::false_type{});
  }

  constexpr var_impl& operator=(var_impl&& o) noexcept
  {
    std::move(o).visit([this]<typename T>(T&& value) { assign(std::move(value)); }, std::false_type{});
    return *this;
  }

  constexpr var_impl(var_impl const& o) noexcept
  {
    o.visit([this]<typename T>(T const& value) { assign(value); }, std::false_type{});
  }

  constexpr var_impl& operator=(var_impl const& o) noexcept
  {
    o.visit([this]<typename T>(T const& value) { assign(value); }, std::false_type{});
    return *this;
  }

  template <typename T> static consteval bool convertible_from()
  {
    return (decay(^^T) != ^^var_impl) and (constructible_from<T>() or explicitly_constructible_from<T>());
  }

  template <typename Var>
    requires(is_var_type(^^Var) and not convertible_from<Var>())
  constexpr var_impl(Var&& o) noexcept
  {
    reflex_fwd(o).visit(match{[this]<typename T>
                                requires(constructible_from<T>())
                              (T&& value) {//
                                 assign(reflex_fwd(value));
                              },
                              patterns::ignore}, std::false_type{});
  }

  template <typename Var>
    requires(is_var_type(^^Var) and not convertible_from<Var>())
  constexpr var_impl& operator=(Var&& o) noexcept
  {
    reflex_fwd(o).visit(match{[this]<typename T>
                                requires(constructible_from<T>())
                              (T&& value) {//
                                 assign(reflex_fwd(value));
                              },
                              patterns::ignore}, std::false_type{});
    return *this;
  }

  constexpr ~var_impl() noexcept
    requires(not trivially_destructible_)
  {
    reset();
  }

  constexpr ~var_impl() noexcept
    requires(trivially_destructible_)
  = default;

  template <typename T>
    requires(convertible_from<T>())
  constexpr var_impl(T&& value) noexcept
  {
    if constexpr(constructible_from<T>())
    {
      assign(reflex_fwd(value));
    }
    else
    {
      template for(constexpr auto R : alternatives_)
      {
        if constexpr(meta::has_explicit_constructor(R, {^^T}))
        {
          using U = [:R:];
          emplace<U>(std::forward<T>(value));
          return;
        }
      }
      std::unreachable();
    }
  }

  template <typename T>
    requires(convertible_from<T>())
  constexpr var_impl& operator=(T&& value) noexcept
  {
    if constexpr(constructible_from<T>())
    {
      assign(std::forward<T>(value));
      return *this;
    }
    else
    {
      template for(constexpr auto R : alternatives_)
      {
        if constexpr(meta::has_explicit_constructor(R, {^^T}))
        {
          using U = [:R:];
          emplace<U>(std::forward<T>(value));
          return *this;
        }
      }
      std::unreachable();
    }
  }

  template <typename T, typename... Args>
    requires(constructible_from<T>() or explicitly_constructible_from<T>())
  constexpr T& emplace(Args&&... args) noexcept
  {
    reset();
    template for(constexpr auto index : indexes_)
    {
      constexpr auto R = alternatives_[index];
      if constexpr(R == ^^T)
      {
        current_ = index;

        new(&storage_) storage_t(std::index_sequence<index>{}, std::forward<Args>(args)...);
        return storage_.template get<T>();
      }
    }
    std::unreachable();
  }

  template <typename T>
    requires(constructible_from<T>() or explicitly_constructible_from<T>())
  constexpr void assign(T&& value) noexcept
  {
    constexpr auto dt = dealias(decay(^^T));

    index_type index = 0;
    template for(constexpr auto index : indexes_)
    {
      constexpr auto R = alternatives_[index];
      if constexpr(R == dt)
      {
        if(current_ != index)
        {
          reset();
        }
        new(&storage_) storage_t(std::index_sequence<index>{}, std::forward<T>(value));
        current_ = index;
        return;
      }
    }
    std::unreachable();
  }

  template <meta::info I, typename Self> constexpr decltype(auto) get(this Self&& self)
  {
    template for(constexpr auto index : indexes_)
    {
      constexpr auto R = self.alternatives_[index];
      if constexpr(is_equivalent_to(I, R))
      {
        if(index == self.current_)
        {
          using T = [:R:];
          if constexpr(is_reference_wrapper_type(R) and not is_reference_wrapper_type(decay(I)))
          {
            return std::forward_like<Self>(self.storage_.template get<T>().get());
          }
          else
          {
            return std::forward_like<Self>(self.storage_.template get<T>());
          }
        }
      }
    }
    throw bad_var_access{};
  }

  template <typename T, typename Self> constexpr decltype(auto) get(this Self&& self)
  {
    return std::forward<Self>(self).template get<^^T>();
  }

  template <template <typename...> typename T, typename Self> constexpr decltype(auto) get(this Self&& self)
  {
    return std::forward<Self>(self).template get<^^T>();
  }

  template <typename T>
    requires(constructible_from<T>())
  constexpr bool operator==(T&& value) const noexcept
  {
    constexpr auto dt = dealias(decay(^^T));
    template for(constexpr auto index : indexes_)
    {
      if(current_ == index)
      {
        constexpr auto R = alternatives_[index];
        using U          = [:R:];
        if constexpr(R == dt)
        {
          return storage_.template get<U>() == value;
        }
        else if constexpr(is_reference_wrapper_type(R) and not is_reference_wrapper_type(decay(^^T)))
        {
          return storage_.template get<U>().get() == value;
        }
      }
    }
    return false;
  }

  template <typename T> static constexpr bool equality_comparable_with() noexcept
  {
    constexpr auto R = dealias(decay(^^T));

    template for(constexpr auto index : indexes_)
    {
      constexpr auto R = alternatives_[index];
      using U          = [:R:];
      if constexpr(std::equality_comparable_with<U, T>)
      {
        return true;
      }
    }
    return false;
  }

  template <typename T>
    requires(not constructible_from<T>() and equality_comparable_with<T>())
  constexpr bool operator==(T&& value) const noexcept
  {
    constexpr auto R = dealias(decay(^^T));
    template for(constexpr auto index : indexes_)
    {
      constexpr auto R = alternatives_[index];
      using U          = [:R:];
      if constexpr(std::equality_comparable_with<U, T>)
      {
        return storage_.template get<U>() == value;
      }
    }
    std::unreachable();
  }

  friend constexpr bool operator==(var_impl const& self, var_impl const& other) noexcept
  {
    if(other.current_ == self.current_)
    {
      template for(constexpr auto index : indexes_)
      {
        constexpr auto R = alternatives_[index];
        if(self.current_ == index)
        {
          return other == self.storage_.template get<typename[:R:]>();
        }
      }
    }
    return false;
  }

  template <typename... Us>
  friend constexpr bool operator==(var_impl const& self, var_impl<Us...> const& other) noexcept
  {
    template for(constexpr auto index : indexes_)
    {
      constexpr auto R = alternatives_[index];
      using T          = [:R:];
      if constexpr(other.template can_hold<T>())
      {
        if(self.current_ == index)
        {
          return other == self.storage_.template get<T>();
        }
      }
      else
      {
        if(self.current_ == index)
        {
          return false;
        }
      }
    }
    return false;
  }

  template <typename T>
    requires(constructible_from<T>())
  operator T() const&
  {
    return get<T>();
  }

  template <typename Self>
    requires(has_vec_)
  decltype(auto) vec(this Self&& self)
  {
    return std::forward<Self>(self).template get<vec_t>();
  }

  template <typename Self>
    requires(has_vec_)
  constexpr decltype(auto) operator[](this Self&& self, size_t index)
  {
    const_like_t<Self, vec_t>* v;
    if(not self.template has_value<vec_t>())
    {
      v = &self.template emplace<vec_t>();
    }
    else
    {
      v = &std::forward<Self>(self).vec();
    }
    if(v->size() < index + 1)
    {
      v->resize(index + 1);
    }
    return (*v)[index];
  }

  template <typename Self>
    requires(has_vec_)
  decltype(auto) at(this Self&& self, size_t index)
  {
    return std::forward<Self>(self).vec().at(index);
  }

  template <typename T>
    requires(has_vec_)
  void push_back(T&& value)
  {
    vec_t* v;
    if(not has_value<vec_t>())
    {
      v = &emplace<vec_t>();
    }
    else
    {
      v = &vec();
    }
    v->push_back(std::forward<T>(value));
  }

  template <typename Self>
    requires(has_map_)
  decltype(auto) map(this Self&& self)
  {
    return std::forward<Self>(self).template get<map_t>();
  }

  template <typename Self, typename KeyT>
    requires(has_map_)
  constexpr decltype(auto) operator[](this Self&& self, KeyT&& key)
    requires(std::constructible_from<typename map_t::key_type, KeyT>)
  {
    const_like_t<Self, map_t>* m;
    if(not self.template has_value<map_t>())
    {
      m = &self.template emplace<map_t>();
    }
    else
    {
      m = &std::forward<Self>(self).map();
    }
    if constexpr(requires { (*m)[key]; })
    {
      return (*m)[key];
    }
    else
    {
      return (*m)[typename map_t::key_type{key}];
    }
  }

  template <typename Self, typename KeyT>
    requires(has_map_)
  decltype(auto) at(this Self&& self, KeyT&& key)
    requires(std::constructible_from<typename map_t::key_type, KeyT>)
  {
    return std::forward<Self>(self).map().at(key);
  }

#define REFLEX_VAR_X_OPS(X) X(+) X(-) X(*)

#define X(__op__)                                                                                          \
  template <typename Self, typename Other> decltype(auto) operator __op__(this Self&& self, Other&& other) \
  {                                                                                                        \
    var_impl result;                                                                                       \
    reflex::visit(                                                                                         \
        match{                                                                                             \
            [&result](auto&& lhs, auto&& rhs)                                                              \
              requires requires { result = lhs __op__ rhs; } { result = lhs __op__ rhs; },                 \
            patterns::ignore,                                                                              \
            },                                                                                             \
            reflex_fwd(self),                                                                              \
            reflex_fwd(other));                                                                            \
    return result;                                                                                         \
  }                                                                                                        \
  template <decays_to_c<var_impl> Self, typename Other>                                                    \
    requires(not meta::is_template_instance_of(decay(^^Other), template_of(^^var_impl)))                   \
  friend decltype(auto) operator __op__(Other&& other, Self&& self)                                        \
  {                                                                                                        \
    std::decay_t<Other> result;                                                                            \
    reflex::visit(                                                                                         \
        match{                                                                                             \
            [&result](auto&& lhs, auto&& rhs)                                                              \
              requires requires { result = lhs __op__ rhs; } { result = lhs __op__ rhs; },                 \
            patterns::ignore,                                                                              \
            },                                                                                             \
            reflex_fwd(other),                                                                             \
            reflex_fwd(self));                                                                             \
    return result;                                                                                         \
  }
  REFLEX_VAR_X_OPS(X)
#undef X

#define REFLEX_VAR_X_NZERO_OPS(X) X(/) X(%)

#define X(__op__)                                                                                          \
  template <typename Self, typename Other> decltype(auto) operator __op__(this Self&& self, Other&& other) \
  {                                                                                                        \
    var_impl result;                                                                                       \
    reflex::visit(                                                                                         \
        match{                                                                                             \
            [&result]<typename L, typename R>(L&& lhs, R&& rhs)                                            \
              requires(not decays_to_c<R, none_t>) and requires { result = lhs __op__ rhs; }               \
            {                                                                                              \
              if(rhs != 0)                                                                                 \
              {                                                                                            \
                result = lhs __op__ rhs;                                                                   \
              }                                                                                            \
            },                                                                                             \
            patterns::ignore,                                                                              \
            },                                                                                             \
            reflex_fwd(self),                                                                              \
            reflex_fwd(other));                                                                            \
    return result;                                                                                         \
  }                                                                                                        \
  template <decays_to_c<var_impl> Self, typename Other>                                                    \
    requires(not meta::is_template_instance_of(decay(^^Other), template_of(^^var_impl)))                   \
  friend decltype(auto) operator __op__(Other&& other, Self&& self)                                        \
  {                                                                                                        \
    std::decay_t<Other> result;                                                                            \
    reflex::visit(                                                                                         \
        match{                                                                                             \
            [&result]<typename L, typename R>(L&& lhs, R&& rhs)                                            \
              requires(not decays_to_c<R, none_t>) and requires { result = lhs __op__ rhs; }               \
            {                                                                                              \
              if(rhs != 0)                                                                                 \
              {                                                                                            \
                result = lhs __op__ rhs;                                                                   \
              }                                                                                            \
            },                                                                                             \
            patterns::ignore,                                                                              \
            },                                                                                             \
            reflex_fwd(other),                                                                             \
            reflex_fwd(self));                                                                             \
    return result;                                                                                         \
  }
  REFLEX_VAR_X_NZERO_OPS(X)
#undef X

#define REFLEX_VAR_X_SELF_OPS(X) X(+=) X(-=) X(*=)

#define X(__op__)                                                                                          \
  template <typename Self, typename Other> decltype(auto) operator __op__(this Self&& self, Other&& other) \
  {                                                                                                        \
    reflex::visit(                                                                                         \
        match{                                                                                             \
            [](auto&& lhs, auto&& rhs)                                                                     \
              requires requires { lhs __op__ rhs; } { lhs __op__ rhs; },                                   \
            patterns::ignore,                                                                              \
            },                                                                                             \
            reflex_fwd(self),                                                                              \
            reflex_fwd(other));                                                                            \
    return self;                                                                                           \
  }
  REFLEX_VAR_X_SELF_OPS(X)
#undef X

#define REFLEX_VAR_X_SELF_NZERO_OPS(X) X(/=) X(%=)

#define X(__op__)                                                                                          \
  template <typename Self, typename Other> decltype(auto) operator __op__(this Self&& self, Other&& other) \
  {                                                                                                        \
    reflex::visit(                                                                                         \
        match{                                                                                             \
            []<typename L, typename R>(L&& lhs, R&& rhs)                                                   \
              requires(not decays_to_c<R, none_t>) and requires { lhs __op__ rhs; }                        \
            {                                                                                              \
              if(rhs != 0)                                                                                 \
              {                                                                                            \
                lhs __op__ rhs;                                                                            \
              }                                                                                            \
            },                                                                                             \
            patterns::ignore,                                                                              \
            },                                                                                             \
            reflex_fwd(self),                                                                              \
            reflex_fwd(other));                                                                            \
    return self;                                                                                           \
  }
  REFLEX_VAR_X_SELF_NZERO_OPS(X)
#undef X
};
} // namespace _var

template <typename... Ts> using var = _var::var_impl<Ts...>;

template <typename... Ts> using recursive_var = _var::var_impl<Ts..., recursive<std::vector>, recursive<std::map>>;

template <typename T, typename Var>
concept var_value_c = Var::template can_hold<std::decay_t<T>>();

} // namespace reflex

template <typename CharT, typename... Ts> struct std::formatter<reflex::_var::var_impl<Ts...>, CharT>
{
  using var_type = reflex::_var::var_impl<Ts...>;

  enum class mode
  {
    none,
    json,
    pretty,
  };

  mode   mode_   = mode::none;
  size_t indent_ = 0;

  inline constexpr bool is_json() const noexcept
  {
    return mode(int(mode_) & int(mode::json)) != mode::none;
  }

  inline constexpr bool is_pretty() const noexcept
  {
    return mode(int(mode_) & int(mode::pretty)) != mode::none;
  }

  constexpr auto parse(auto& ctx)
  {
    auto it = ctx.begin();
    if(it == ctx.end())
      return it;
    while(it != ctx.end() && *it != '}')
    {
      if(*it == 'j')
      {
        mode_ = mode(int(mode_) | int(mode::json));
      }
      else if(*it == 'p')
      {
        mode_ = mode(int(mode_) | int(mode::pretty));
      }
      else if(reflex::is_digit(*it))
      {
        if(not is_pretty())
        {
          mode_ = mode(int(mode_) | int(mode::pretty));
        }
        if(indent_ != 0)
        {
          throw std::format_error("Invalid format args for reflex::var: indent value has only 1 digit.");
        }
        indent_ = *it - '0';
      }
      else
      {
        throw std::format_error("Invalid format args for reflex::var.");
      }
      ++it;
      if(is_pretty() and indent_ == 0)
      {
        indent_ = 2; // default to 2
      }
    }
    return it;
  }

  template <typename Ctx> auto format(var_type const& c, Ctx& ctx, size_t indent = 0) const -> decltype(ctx.out())
  {
    auto do_close_indent = [indent](auto& out) { return std::format_to(out, "\n{:{}}", "", indent); };
    indent += indent_;
    auto do_indent = [indent](auto& out) { return std::format_to(out, "\n{:{}}", "", indent); };
    return reflex::visit( //
        [&]<typename T>(T const& value)
        {
          constexpr auto type = dealias(std::meta::decay(^^T));
          if constexpr(std::ranges::contains(c.recursive_types_, type) and requires { std::begin(value); })
          {
            constexpr auto yielded_type = dealias(std::meta::decay(^^decltype(*std::begin(value))));
            using YieldedT              = [:yielded_type:];
            if constexpr(reflex::meta::is_template_instance_of(yielded_type, ^^std::pair))
            {
              auto out = ctx.out();
              *out++   = '{';
              if(is_pretty())
              {
                out = do_indent(out);
              }
              bool first = true;
              for(const auto& [k, v] : value)
              {
                if(!first) [[likely]]
                {
                  *out++ = ',';
                  if(is_pretty())
                  {
                    out = do_indent(out);
                  }
                  else
                  {
                    *out++ = ' ';
                  }
                }
                else
                {
                  first = false;
                }
                if(is_json())
                {
                  out = std::format_to(ctx.out(), "\"{}\": ", k);
                }
                else
                {
                  out = std::format_to(ctx.out(), "{}: ", k);
                }
                out = this->format(v, ctx, indent);
              }
              if(is_pretty())
              {
                out = do_close_indent(out);
              }
              *out++ = '}';
              return out;
            }
            else
            {
              auto out = ctx.out();
              *out++   = '[';
              if(is_pretty())
              {
                out = do_indent(out);
              }
              bool first = true;
              for(const auto& v : value)
              {
                if(!first) [[likely]]
                {
                  *out++ = ',';
                  if(is_pretty())
                  {
                    out = do_indent(out);
                  }
                  else
                  {
                    *out++ = ' ';
                  }
                }
                else
                {
                  first = false;
                }
                out = this->format(v, ctx, indent);
              }
              if(is_pretty())
              {
                out = do_close_indent(out);
              }
              *out++ = ']';
              return out;
            }
          }
          else if constexpr(requires { std::formatter<T>{}; })
          {
            if constexpr(requires {
                           { value.data() } -> std::same_as<const char*>;
                         })
            {
              if(is_json())
              {
                return std::format_to(ctx.out(), "\"{}\"", value);
              }
            }
            else if constexpr(reflex::none_c<T>)
            {
              if(is_json())
              {
                // none is represented as null in json
                return std::format_to(ctx.out(), "null", value);
              }
            }
            return std::format_to(ctx.out(), "{}", value);
          }
          else
          {
            return std::format_to(ctx.out(), "<unformattable[{}]>", display_string_of(^^T));
          }
        },
        c);
  }
};
