#pragma once

#include <reflex/meta.hpp>
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
template <class... Ts> struct match_pattern : Ts...
{
  using Ts::operator()...;
};
template <class... Ts> match_pattern(Ts...) -> match_pattern<Ts...>;

struct bad_var_access : std::exception
{
  const char* what() const noexcept final
  {
    return "bad var";
  }
};

namespace patterns
{
template <auto return_value>
inline constexpr auto unreachable_r = [](auto&&...)
{
  std::unreachable();
  return return_value;
};
inline constexpr auto unreachable = [](auto&&...) { std::unreachable(); };
} // namespace patterns

namespace _var
{

template <auto __make_recursive_types = [](meta::info var_type) { return std::vector<meta::info>{}; }> struct config_t
{
  static constexpr std::vector<meta::info> make_recursive_types(meta::info V)
  {
    return __make_recursive_types(V);
  }
};

template <config_t config, meta::info... types> class var_impl
{
public:
  using index_type = int;

  friend std::formatter<var_impl>;

private:
  static constexpr auto config_ = config;
  static constexpr auto ctx_    = meta::access_context::current();

  static constexpr auto recursive_types_ = define_static_array(config.make_recursive_types(^^var_impl));
  static constexpr auto recursive_types_template_ =
      define_static_array(recursive_types_ | std::views::transform(meta::template_of));

  static constexpr auto vec_type_ = [] constexpr
  {
    template for(constexpr auto type : recursive_types_)
    {
      using T = [:type:];
      if constexpr(requires(T v) {
                     v.begin();
                     v.end();
                     typename T::value_type;
                   })
      {
        return type;
      }
    }
    return ^^void;
  }();

  static constexpr auto map_type_ = [] constexpr
  {
    template for(constexpr auto type : recursive_types_)
    {
      using T = [:type:];
      if constexpr(requires(T v) {
                     v.begin();
                     v.end();
                     typename T::key_type;
                     typename T::value_type;
                     typename T::mapped_type;
                   })
      {
        return type;
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

  static constexpr auto alternatives_ = define_static_array(
      [] consteval
      {
        auto types_impl =
            std::array{types...} | std::views::transform(type_implementation_of) | std::ranges::to<std::vector>();
        types_impl.append_range(recursive_types_);
        // if(has_vec_)
        // {
        //   types_impl.push_back(type_implementation_of(config.vec_template));
        // }
        // if(has_map_)
        // {
        //   types_impl.push_back(type_implementation_of(config.map_template));
        // }
        return types_impl;
      }());

  static constexpr auto storage_size_ = [] constexpr
  {
    size_t sz = 0;
    template for(constexpr auto A : alternatives_)
    {
      sz = std::max(sz, sizeof(typename[:A:]));
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
    return sz;
  }();

  alignas(storage_align_) std::byte storage_[storage_size_];
  index_type current_ = -1;

public:
  using vec_t = [:vec_type_:];
  using map_t = [:map_type_:];

  constexpr void reset() noexcept
  {
    if(current_ != -1)
    {
      index_type index = 0;
      template for(constexpr auto R : alternatives_)
      {
        if(index == current_)
        {
          using T = [:R:];
          std::destroy_at(reinterpret_cast<T*>(storage_));
          break;
        }
        ++index;
      }
    }
    current_ = -1;
  }

  constexpr bool has_value() noexcept
  {
    return current_ != -1;
  }

  static consteval bool is_equivalent_to(meta::info I, meta::info R) noexcept
  {
    return ((R == dealias(I)) or (is_template(I) and has_template_arguments(R) and template_of(R) == I));
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
    return index_type(-1);
  }

  template <typename T> constexpr bool has_value() noexcept
  {
    return current_ == index_of<T>();
  }

  template <template <typename ...> typename T> constexpr bool has_value() noexcept
  {
    return current_ == index_of<typename[:type_implementation_of(^^T):]>();
  }

  constexpr var_impl() = default;

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
    assign(std::forward<T>(value));
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
    index_type index = 0;
    template for(constexpr auto R : alternatives_)
    {
      if constexpr(R == ^^T)
      {
        auto* p = reinterpret_cast<typename[:R:]*>(storage_);
        if(current_ != index)
        {
          reset();
        }
        current_ = index;
        return *std::construct_at<T>(p, std::forward<Args>(args)...);
      }
      ++index;
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
        break;
      }
      ++index;
    }
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
    return false;
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
    return std::forward<Self>(self).vec()[index];
  }

  template <typename Self>
    requires(has_vec_)
  decltype(auto) at(this Self&& self, size_t index)
  {
    return std::forward<Self>(self).vec().at(index);
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
    return std::forward<Self>(self).map()[key];
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

constexpr auto __make_recursive_var_types = [](meta::info V)
{
  return std::vector<meta::info>{substitute(^^std::vector,
                                            {
                                                V}),
                                 substitute(^^std::map,
                                            {
                                                ^^std::string,
                                                V})};
};

template <typename... Ts> using recursive_var = _var::var_impl<_var::config_t<__make_recursive_var_types>{}, ^^Ts...>;

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
    json
  };

  mode mode_ = mode::none;

  constexpr auto parse(auto& ctx)
  {
    auto it = ctx.begin();
    if(it == ctx.end())
      return it;
    while(it != ctx.end() && *it != '}')
    {
      if(*it == 'j')
      {
        mode_ = mode::json;
      }
      else
      {
        throw std::format_error("Invalid format args for reflex::var.");
      }
      ++it;
    }
    return it;
  }

  template <typename Ctx> auto format(var_type const& c, Ctx& ctx) const -> decltype(ctx.out())
  {
    return c.match( //
        [&]<typename T>(T const& value)
        {
          constexpr auto type = dealias(std::meta::decay(^^T));
          if constexpr(std::ranges::contains(c.recursive_types_, type))
          {
            constexpr auto yielded_type = dealias(std::meta::decay(^^decltype(*std::begin(value))));
            using YieldedT              = [:yielded_type:];
            if constexpr(has_template_arguments(yielded_type) and template_of(yielded_type) == ^^std::pair)
            {
              auto out   = ctx.out();
              *out++     = '{';
              bool first = true;
              for(const auto& [k, v] : value)
              {
                if(!first) [[likely]]
                {
                  *out++ = ',';
                  *out++ = ' ';
                }
                else
                {
                  first = false;
                }
                if(mode_ == mode::json)
                {
                  out = std::format_to(ctx.out(), "\"{}\": ", k);
                }
                else
                {
                  out = std::format_to(ctx.out(), "{}: ", k);
                }
                out = this->format(v, ctx);
              }
              *out++ = '}';
              return out;
            }
            else
            {
              auto out   = ctx.out();
              *out++     = '[';
              bool first = true;
              for(const auto& v : value)
              {
                if(!first) [[likely]]
                {
                  *out++ = ',';
                  *out++ = ' ';
                }
                else
                {
                  first = false;
                }
                out = this->format(v, ctx);
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
              if(mode_ == mode::json)
              {
                return std::format_to(ctx.out(), "\"{}\"", value);
              }
            }
            return std::format_to(ctx.out(), "{}", value);
          }
        });
  }
};
