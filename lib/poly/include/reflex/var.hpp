#pragma once

#include <reflex/match_patten.hpp>
#include <reflex/meta.hpp>
#include <reflex/none.hpp>
#include <reflex/utility.hpp>

#include <array>
#include <exception>
#include <flat_map>
#include <format>
#include <map>
#include <unordered_map>
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

namespace _var
{

struct config_t
{
  meta::info recursive_vec_template = meta::null;
  meta::info recursive_map_template = meta::null;
};

template <config_t config, meta::info... types> class var_impl
{
public:
  using index_type = int;

  friend std::formatter<var_impl>;

private:
  static constexpr auto config_ = config;
  static constexpr auto ctx_    = meta::access_context::current();

  static constexpr auto vec_type_ = [] consteval
  {
    if constexpr(config.recursive_vec_template != meta::null)
    {
      return substitute(config.recursive_vec_template, {^^var_impl});
    }
    else
    {
      return ^^void;
    }
  }();

  static constexpr auto map_type_ = [] consteval
  {
    if constexpr(config.recursive_map_template != meta::null)
    {
      return substitute(config.recursive_map_template, {^^std::string, ^^var_impl});
    }
    else
    {
      return ^^void;
    }
  }();

  static constexpr auto recursive_types_ = define_static_array(
      [] consteval
      {
        std::vector<meta::info> rt;
        if constexpr(vec_type_ != ^^void)
        {
          rt.push_back(vec_type_);
        }
        if constexpr(map_type_ != ^^void)
        {
          rt.push_back(map_type_);
        }
        return rt;
      }());

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

  static constexpr auto alternatives_ = std::array{^^none_t, types...};

  static constexpr size_t vec_index_ = has_vec_ ? alternatives_.size() : -1;
  static constexpr size_t map_index_ = has_map_ ? has_vec_ ? vec_index_ + 1 : alternatives_.size() : -1;

  // static constexpr auto alternatives_ = define_static_array([] consteval
  // {
  //   auto tps = std::vector{^^none_t, types...};
  //   if(has_vec_)
  //   {
  //     tps.push_back(vec_type_);
  //   }
  //   if(has_map_)
  //   {
  //     tps.push_back(map_type_);
  //   }
  //   return tps;
  // }());

  static constexpr auto storage_size_ = [] constexpr
  {
    size_t sz = 0;
    template for(constexpr auto A : alternatives_)
    {
      sz = std::max(sz, sizeof(typename[:A:]));
    }
    if constexpr(has_vec_)
    {
      sz = std::max(sz, sizeof(typename[:vec_type_:]));
    }
    if constexpr(has_map_)
    {
      sz = std::max(sz, sizeof(typename[:map_type_:]));
    }
    return sz;
  }();
  static constexpr auto storage_align_ = [] constexpr
  {
    size_t sz = 0;
    template for(constexpr auto A : alternatives_)
    {
      sz = std::max(sz, alignof(typename[:A:]));
    }
    if constexpr(has_vec_)
    {
      sz = std::max(sz, alignof(typename[:vec_type_:]));
    }
    if constexpr(has_map_)
    {
      sz = std::max(sz, alignof(typename[:map_type_:]));
    }
    return sz;
  }();

  alignas(storage_align_) std::byte storage_[storage_size_];
  index_type current_ = 0;

public:
  using vec_t = [:vec_type_:];
  using map_t = [:map_type_:];

  constexpr void reset() noexcept
  {
    if(current_ == -1)
    {
      return;
    }
    index_type index = 0;
    template for(constexpr auto R : alternatives_)
    {
      if(index == current_)
      {
        using T = [:R:];
        std::destroy_at(reinterpret_cast<T*>(storage_));
        current_ = -1;
        return;
      }
      ++index;
    }
    if constexpr(has_vec_)
    {
      if(vec_index_ == current_)
      {
        std::destroy_at(reinterpret_cast<typename[:vec_type_:]*>(storage_));
        current_ = -1;
        return;
      }
    }
    if constexpr(has_map_)
    {
      if(map_index_ == current_)
      {
        std::destroy_at(reinterpret_cast<typename[:map_type_:]*>(storage_));
        current_ = -1;
        return;
      }
    }
  }

  constexpr bool has_value() noexcept
  {
    return current_ > 0;
  }

  static consteval bool is_equivalent_to(meta::info I, meta::info R) noexcept
  {
    return ((R == dealias(I)) or (is_template(I) and meta::is_template_instance_of(R, I)));
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
      if constexpr(R == ^^T)
      {
        return index;
      }
      ++index;
    }
    if constexpr(vec_type_ == ^^T)
    {
      return vec_index_;
    }
    if constexpr(map_type_ == ^^T)
    {
      return map_index_;
    }
    return index_type(-1);
  }

  template <typename T> constexpr bool has_value() noexcept
  {
    return current_ == index_of<T>();
  }

  template <template <typename...> typename T> constexpr bool has_value() noexcept
  {
    return current_ == index_of<typename[:type_implementation_of(^^T):]>();
  }

  constexpr var_impl() noexcept
  {
    emplace<none_t>();
  }

  constexpr var_impl(var_impl&& o) noexcept
  {
    std::move(o).match([this]<typename T>(T&& value) { assign(std::move(value)); });
  }
  constexpr var_impl(var_impl const& o) noexcept
  {
    o.match([this]<typename T>(T const& value) { assign(value); });
  }

  constexpr var_impl& operator=(var_impl&& o) noexcept
  {
    std::move(o).match([this]<typename T>(T&& value) { assign(std::move(value)); });
    return *this;
  }

  constexpr var_impl& operator=(var_impl const& o) noexcept
  {
    o.match([this]<typename T>(T const& value) { assign(value); });
    return *this;
  }

  constexpr ~var_impl() noexcept
  {
    reset();
  }

  template <typename T>
    requires(constructible_from<T>())
  constexpr var_impl(T&& value) noexcept
  {
    emplace<std::decay_t<T>>(std::forward<T>(value));
  }

  template <typename T>
    requires((decay(^^T) != ^^var_impl) and not constructible_from<T>() and explicitly_constructible_from<T>())
  constexpr var_impl(T&& value) noexcept
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

  template <typename T>
    requires(constructible_from<T>())
  constexpr var_impl& operator=(T&& value) noexcept
  {
    assign(std::forward<T>(value));
    return *this;
  }

  template <typename T, typename... Args>
    requires(constructible_from<T>())
  constexpr T& emplace(Args&&... args) noexcept
  {
    reset();
    index_type index = 0;
    template for(constexpr auto R : alternatives_)
    {
      if constexpr(R == ^^T)
      {
        current_ = index;
        auto* p  = reinterpret_cast<typename[:R:]*>(storage_);
        return *std::construct_at<T>(p, std::forward<Args>(args)...);
      }
      ++index;
    }
    if constexpr(^^T == vec_type_)
    {
      using U  = [:vec_type_:];
      current_ = vec_index_;
      auto* p  = reinterpret_cast<U*>(storage_);
      return *std::construct_at<U>(p, std::forward<Args>(args)...);
    }
    if constexpr(^^T == map_type_)
    {
      using U  = [:map_type_:];
      current_ = map_index_;
      auto* p  = reinterpret_cast<U*>(storage_);
      return *std::construct_at<U>(p, std::forward<Args>(args)...);
    }
    std::unreachable();
  }

  template <typename T>
    requires(constructible_from<T>())
  constexpr void assign(T&& value) noexcept
  {
    constexpr auto dt = dealias(decay(^^T));

    index_type index = 0;
    template for(constexpr auto R : alternatives_)
    {
      if constexpr(R == dt)
      {
        auto* p = reinterpret_cast<typename[:R:]*>(storage_);
        if(current_ != index)
        {
          reset();
          std::construct_at<std::decay_t<T>>(p, std::forward<T>(value));
        }
        else
        {
          *p = std::forward<T>(value);
        }
        current_ = index;
        return;
      }
      ++index;
    }
    if constexpr(dt == vec_type_)
    {
      using U = [:vec_type_:];
      auto* p = reinterpret_cast<U*>(storage_);
      if(current_ != vec_index_)
      {
        reset();
        std::construct_at<std::decay_t<T>>(p, std::forward<T>(value));
      }
      else
      {
        *p = std::forward<T>(value);
      }
      current_ = vec_index_;
      return;
    }
    if constexpr(dt == map_type_)
    {
      using U = [:map_type_:];
      auto* p = reinterpret_cast<U*>(storage_);
      if(current_ != map_index_)
      {
        reset();
        std::construct_at<std::decay_t<T>>(p, std::forward<T>(value));
      }
      else
      {
        *p = std::forward<T>(value);
      }
      current_ = map_index_;
      return;
    }
    std::unreachable();
  }

  template <typename... Patterns, typename Self>
  decltype(auto) match(this Self&& self, match_pattern<Patterns...>&& patterns)
  {
    template for(constexpr auto ii : std::views::iota(size_t(0), self.alternatives_.size()))
    {
      constexpr auto R = self.alternatives_[ii];
      auto*          p = reinterpret_cast<const_like_t<Self, typename[:R:]>*>(self.storage_);
      if constexpr(requires { patterns(std::forward_like<Self>(*p)); })
      {
        if(ii == self.current_)
        {
          return patterns(std::forward_like<Self>(*p));
        }
      }
      else
      {
        if(ii == self.current_)
        {
          throw bad_var_access{}; // no matching pattern
        }
      }
    }
    if constexpr(has_vec_)
    {
      if(self.current_ == vec_index_)
      {
        using U = [:vec_type_:];
        auto* p = reinterpret_cast<const_like_t<Self, U>*>(self.storage_);
        if constexpr(requires { patterns(std::forward_like<Self>(*p)); })
        {
          return patterns(std::forward_like<Self>(*p));
        }
        else
        {
          throw bad_var_access{}; // no matching pattern
        }
      }
    }
    if constexpr(has_map_)
    {
      if(self.current_ == map_index_)
      {
        using U = [:map_type_:];
        auto* p = reinterpret_cast<const_like_t<Self, U>*>(self.storage_);
        if constexpr(requires { patterns(std::forward_like<Self>(*p)); })
        {
          return patterns(std::forward_like<Self>(*p));
        }
        else
        {
          throw bad_var_access{}; // no matching pattern
        }
      }
    }
    std::unreachable();
  }

  template <typename... Patterns, typename Self> decltype(auto) match(this Self&& self, Patterns&&... patterns) noexcept
  {
    return std::forward<Self>(self).match(match_pattern{std::forward<Patterns>(patterns)...});
  }

  template <meta::info I, typename Self> constexpr decltype(auto) get(this Self&& self)
  {
    template for(constexpr auto ii : std::views::iota(size_t(0), self.alternatives_.size()))
    {
      constexpr auto R = self.alternatives_[ii];
      if constexpr(is_equivalent_to(I, R))
      {
        if(ii == self.current_)
        {
          using U = [:type_implementation_of(R):];
          return *reinterpret_cast<const_like_t<Self, U>*>(self.storage_);
        }
      }
    }
    if constexpr(is_equivalent_to(I, vec_type_))
    {
      using U = [:vec_type_:];
      return *reinterpret_cast<const_like_t<Self, U>*>(self.storage_);
    }
    if constexpr(is_equivalent_to(I, map_type_))
    {
      using U = [:map_type_:];
      return *reinterpret_cast<const_like_t<Self, U>*>(self.storage_);
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
    index_type     index = 0;
    constexpr auto dt    = dealias(decay(^^T));
    template for(constexpr auto R : alternatives_)
    {
      if constexpr(R == dt)
      {
        auto* p = reinterpret_cast<const typename[:R:]*>(storage_);
        return current_ == index and *p == value;
      }
      ++index;
    }
    if constexpr(dt == vec_type_)
    {
      using U = [:vec_type_:];
      auto* p = reinterpret_cast<const U*>(storage_);
      return current_ == vec_index_ and *p == value;
    }
    if constexpr(dt == map_type_)
    {
      using U = [:map_type_:];
      auto* p = reinterpret_cast<const U*>(storage_);
      return current_ == map_index_ and *p == value;
    }
    return false;
  }

  template <typename T> static constexpr bool equality_comparable_with() noexcept
  {
    constexpr auto R = dealias(decay(^^T));

    template for(constexpr auto A : alternatives_)
    {
      using U = [:A:];
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
    template for(constexpr auto A : alternatives_)
    {
      using U = [:A:];
      if constexpr(std::equality_comparable_with<U, T>)
      {
        auto* p = reinterpret_cast<const typename[:A:]*>(storage_);
        return value == *p;
      }
    }
    std::unreachable();
  }

  friend constexpr bool operator==(var_impl const& self, var_impl const& other) noexcept
  {
    if(other.current_ == self.current_)
    {
      index_type index = 0;
      template for(constexpr auto R : self.alternatives_)
      {
        if(self.current_ == index)
        {
          auto* op = reinterpret_cast<const typename[:R:]*>(other.storage_);
          auto* p  = reinterpret_cast<const typename[:R:]*>(self.storage_);
          return *op == *p;
        }
        ++index;
      }
      if constexpr(has_vec_)
      {
        if(self.current_ == vec_index_)
        {
          using U  = [:vec_type_:];
          auto* op = reinterpret_cast<const U*>(other.storage_);
          auto* p  = reinterpret_cast<const U*>(self.storage_);
          return *op == *p;
        }
      }
      if constexpr(has_map_)
      {
        if(self.current_ == map_index_)
        {
          using U  = [:map_type_:];
          auto* op = reinterpret_cast<const U*>(other.storage_);
          auto* p  = reinterpret_cast<const U*>(self.storage_);
          return *op == *p;
        }
      }
    }
    return false;
  }

  template <config_t ocfg, meta::info... otypes>
  friend constexpr bool operator==(var_impl const& self, var_impl<ocfg, otypes...> const& other) noexcept
  {
    index_type index = 0;
    template for(constexpr auto R : self.alternatives_)
    {
      using T = [:R:];
      if constexpr(other.template can_hold<T>())
      {
        if(self.current_ == index)
        {
          auto* p = reinterpret_cast<const typename[:R:]*>(self.storage_);
          return other == *p;
        }
      }
      else
      {
        if(self.current_ == index)
        {
          return false;
        }
      }
      ++index;
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
  decltype(auto) operator[](this Self&& self, size_t index)
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
  decltype(auto) operator[](this Self&& self, KeyT&& key)
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
    return (*m)[key];
  }

  template <typename Self, typename KeyT>
    requires(has_map_)
  decltype(auto) at(this Self&& self, KeyT&& key)
    requires(std::constructible_from<typename map_t::key_type, KeyT>)
  {
    return std::forward<Self>(self).map().at(key);
  }
};
} // namespace _var

template <typename... Ts> using var = _var::var_impl<_var::config_t{}, ^^Ts...>;

template <typename... Ts>
using recursive_var = _var::var_impl<_var::config_t{
                                         .recursive_vec_template = ^^std::vector,
                                         .recursive_map_template = ^^std::map,
                                     },
                                     ^^Ts...>;

template <typename T, typename Var>
concept var_value_c = Var::template can_hold<std::decay_t<T>>();

} // namespace reflex

template <typename CharT, reflex::_var::config_t config, reflex::meta::info... types>
struct std::formatter<reflex::_var::var_impl<config, types...>, CharT>
{
  using var_type = reflex::_var::var_impl<config, types...>;

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
    return c.match( //
        [&]<typename T>(T const& value)
        {
          constexpr auto type = dealias(std::meta::decay(^^T));
          if constexpr(std::ranges::contains(c.recursive_types_, type))
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
          else
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
        });
  }
};
