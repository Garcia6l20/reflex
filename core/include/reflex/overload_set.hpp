#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <meta>
#include <vector>
#endif

#include <reflex/constant.hpp>

REFLEX_EXPORT namespace reflex
{
  struct overload_set_item
  {
    std::meta::info                        return_type     = ^^void;
    constant<std::vector<std::meta::info>> parameter_types = std::vector<std::meta::info>{};

    template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
    consteval bool is_invocable(R&& args) const
    {
      if(parameter_types->size() != std::ranges::size(args))
      {
        return false;
      }
      for(auto [p, a] : std::views::zip(*parameter_types, args))
      {
        const bool convertible = extract<bool>(substitute(
            ^^std::convertible_to, {
                                       a, p}));
        if(not convertible)
        {
          return false;
        }
      }
      return true;
    }

    consteval bool operator==(const overload_set_item& other) const
    {
      if(dealias(return_type) != dealias(other.return_type))
        return false;
      if(parameter_types->size() != other.parameter_types->size())
        return false;

      for(std::size_t i = 0; i < parameter_types->size(); ++i)
      {
        if(dealias((*parameter_types)[i]) != dealias((*other.parameter_types)[i]))
          return false;
      }
      return true;
    }
    consteval auto operator<=>(const overload_set_item& other) const = default;
  };
  struct overload_set
  {
    constant<std::vector<overload_set_item>> items;

    consteval auto begin() const
    {
      return items->begin();
    }
    consteval auto end() const
    {
      return items->end();
    }
    consteval auto size() const
    {
      return items->size();
    }
    consteval auto empty() const
    {
      return items->empty();
    }

    template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
    consteval bool is_invocable(R&& args) const
    {
      for(auto& item : *items)
      {
        if(item.is_invocable(args))
        {
          return true;
        }
      }
      return false;
    }

    template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
    consteval std::meta::info invoke_result(R&& args) const
    {
      for(auto& item : *items)
      {
        if(item.is_invocable(args))
        {
          return item.return_type;
        }
      }
      throw std::meta::exception("No matching overload found", std::meta::info{});
    }

    template <typename... Args> consteval bool is_invocable_with(Args&&...) const
    {
      return is_invocable({^^Args...});
    }
  };
  namespace detail
  {
  template <std::meta::info Fn, std::meta::info... Args>
  constexpr inline bool callable_with_v = requires { [:Fn:](std::declval<typename[:Args:]>()...); };

  template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
  consteval bool callable_with(std::meta::info fn, R&& args)
  {
    if(is_function(fn))
    {
      using namespace std::views;
      using namespace std::meta;
      std::vector<std::meta::info> callable_args = {reflect_constant(fn)};
      callable_args.append_range(args | transform([](auto arg) { return reflect_constant(arg); }));
      return extract<bool>(constant_of(substitute(^^callable_with_v, callable_args)));
    }
    else
    {
      throw std::meta::exception(std::string("Unhandled type: ") + display_string_of(fn), fn);
    }
  }

  template <std::meta::info Fn, std::meta::info... Args>
  constexpr inline bool constructible_with_v =
      requires { typename[:Fn:](std::declval<typename[:Args:]>()...); };

  template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
  consteval bool constructible_with(std::meta::info type, R&& args)
  {
    if(is_class_type(type))
    {
      using namespace std::views;
      using namespace std::meta;
      std::vector<std::meta::info> callable_args = {reflect_constant(type)};
      callable_args.append_range(args | transform([](auto arg) { return reflect_constant(arg); }));
      return extract<bool>(constant_of(substitute(^^constructible_with_v, callable_args)));
    }
    else
    {
      throw std::meta::exception(std::string("Unhandled type: ") + display_string_of(type), type);
    }
  }

  } // namespace detail

  consteval auto overload_set_of(std::meta::info fn)
  {
    std::vector<overload_set_item> overloads;
    if(is_function(fn))
    {
      auto                         return_type   = return_type_of(fn);
      auto                         params        = parameters_of(fn);
      std::vector<std::meta::info> callable_args = {reflect_constant(fn)};
      std::vector<std::meta::info> param_types;
      if(detail::callable_with(fn, {}))
      {
        overloads.push_back({return_type, param_types});
      }
      for(auto p : params)
      {
        param_types.push_back(type_of(p));
        if(detail::callable_with(fn, param_types))
        {
          overloads.push_back({return_type, param_types});
        }
      }
    }
    else
    {
      throw std::meta::exception(std::string("Unhandled type: ") + display_string_of(fn), fn);
    }
    return overload_set{overloads};
  }

  consteval auto constructors_of(
      std::meta::info t, std::meta::access_context ctx = std::meta::access_context::unchecked())
  {
    if(is_class_type(t))
    {
      return members_of(t, ctx) | std::views::filter(std::meta::is_constructor);
    }
    else
    {
      throw std::meta::exception(std::string("Unhandled type: ") + display_string_of(t), t);
    }
  }

  consteval auto overload_set_of_type(std::meta::info t)
  {
    std::vector<overload_set_item> overloads;
    if(is_class_type(t))
    {
      for(auto ctor : constructors_of(t))
      {
        auto                         return_type = t;
        auto                         params      = parameters_of(ctor);
        std::vector<std::meta::info> param_types;
        if(detail::constructible_with(t, {}))
        {
          overload_set_item item{return_type, param_types};
          if(not std::ranges::contains(overloads, item))
          {
            overloads.push_back(item);
          }
        }
        for(auto p : params)
        {
          param_types.push_back(type_of(p));
          if(detail::constructible_with(t, param_types))
          {
            overload_set_item item{return_type, param_types};
            if(not std::ranges::contains(overloads, item))
            {
              overloads.push_back(item);
            }
          }
        }
      }
    }
    else
    {
      throw std::meta::exception(std::string("Unhandled type: ") + display_string_of(t), t);
    }
    return overload_set{overloads};
  }
} // namespace reflex