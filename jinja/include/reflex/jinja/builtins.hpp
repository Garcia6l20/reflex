#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/exception.hpp>

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#endif

#include <reflex/jinja/context.hpp>

REFLEX_EXPORT namespace reflex::jinja::expr
{
  namespace detail
  {

  // === error reporting, one shape for every builtin

  [[noreturn]] inline void builtin_error(std::string_view name, std::string_view what)
  {
    throw runtime_error("{}(): {}", name, what);
  }

  template <typename ValueT>
  void check_arity(
      std::string_view name, std::span<const ValueT> args, std::size_t lo, std::size_t hi)
  {
    if(args.size() < lo or args.size() > hi)
    {
      if(lo == hi)
      {
        builtin_error(
            name,
            std::format("expects {} argument{}, got {}", lo, lo == 1 ? "" : "s", args.size()));
      }
      builtin_error(
          name, std::format("expects {} to {} arguments, got {}", lo, hi, args.size()));
    }
  }

  // === value accessors
  //
  // A value bound by reference lives in the variant as a pointer alternative, so every accessor
  // has to look through `var*` and past the `arr_type*` / `obj_type*` forms. `std::get_if<T>` on
  // its own only ever sees the by-value alternative, which is what strings happen to use.

  template <typename ValueT> const ValueT& deref(const ValueT& v) noexcept
  {
    if(auto* p = std::get_if<ValueT*>(&v))
    {
      return **p;
    }
    return v;
  }

  // Strings bind by value: `std::string&` has no alternative, so there is no pointer form here.
  template <typename ValueT> const std::string* as_string(const ValueT& v) noexcept
  {
    if constexpr(ValueT::template can_hold<std::string>())
    {
      return std::get_if<std::string>(&deref(v));
    }
    else
    {
      return nullptr;
    }
  }

  template <typename ValueT> const typename ValueT::arr_type* as_array(const ValueT& v) noexcept
  {
    using arr_type = typename ValueT::arr_type;
    const auto& d  = deref(v);
    if(auto* a = std::get_if<arr_type>(&d))
    {
      return a;
    }
    if(auto* p = std::get_if<arr_type*>(&d))
    {
      return *p;
    }
    return nullptr;
  }

  template <typename ValueT> const typename ValueT::obj_type* as_object(const ValueT& v) noexcept
  {
    using obj_type = typename ValueT::obj_type;
    const auto& d  = deref(v);
    if(auto* o = std::get_if<obj_type>(&d))
    {
      return o;
    }
    if(auto* p = std::get_if<obj_type*>(&d))
    {
      return *p;
    }
    return nullptr;
  }

  // The string argument of a filter that inspects text. Throws rather than coercing: a filter that
  // silently accepts a number where the author meant a string is the worse failure.
  template <typename ValueT>
  const std::string& string_arg(std::string_view name, const ValueT& v, std::string_view what)
  {
    if(auto* s = as_string(v))
    {
      return *s;
    }
    builtin_error(name, std::format("{} must be a string", what));
  }

  // The scalar text of any value, for the filters that render rather than inspect. Every probe is
  // guarded: `is<T>()` is ill-formed, not false, for a T that is not an alternative, and which
  // alternatives exist depends on what was bound to the context.
  template <typename ValueT> std::string as_text(const ValueT& v)
  {
    const auto& d = deref(v);
    if constexpr(ValueT::template can_hold<std::string>())
    {
      if(d.template is<std::string>())
      {
        return d.template as<std::string>();
      }
    }
    if constexpr(ValueT::template can_hold<bool>())
    {
      if(d.template is<bool>())
      {
        return d.template as<bool>() ? "true" : "false";
      }
    }
    if constexpr(ValueT::template can_hold<int>())
    {
      if(d.template is<int>())
      {
        return std::to_string(d.template as<int>());
      }
    }
    if constexpr(ValueT::template can_hold<std::int64_t>())
    {
      if(d.template is<std::int64_t>())
      {
        return std::to_string(d.template as<std::int64_t>());
      }
    }
    if constexpr(ValueT::template can_hold<double>())
    {
      if(d.template is<double>())
      {
        return std::format("{}", d.template as<double>());
      }
    }
    return {};
  }

  // === the table
  //
  // One table per value type, built on first use. Registering from the context constructor would
  // cost an allocation per name per context.

  template <typename ContextT>
  using builtin_table_type =
      std::unordered_map<std::string_view, typename ContextT::function_type>;

  template <typename ContextT> void register_tier1(builtin_table_type<ContextT>& t)
  {
    using value_type = typename ContextT::value_type;

    // length(x) / count(x) - characters of a string, elements of an array, keys of an object.
    typename ContextT::function_type length = [](std::span<const value_type> args) -> value_type {
      check_arity("length", args, 1, 1);
      if(auto* s = as_string(args[0]))
      {
        return int(s->size());
      }
      if(auto* a = as_array(args[0]))
      {
        return int(a->size());
      }
      if(auto* o = as_object(args[0]))
      {
        return int(o->size());
      }
      builtin_error("length", "expects a string, an array or an object");
    };
    t.emplace("length", std::move(length));
  }

  template <typename ContextT> void register_builtins(builtin_table_type<ContextT>& t)
  {
    register_tier1<ContextT>(t);
  }

  template <typename ContextT> const builtin_table_type<ContextT>& builtin_table()
  {
    static const builtin_table_type<ContextT> table = [] {
      builtin_table_type<ContextT> t;
      register_builtins<ContextT>(t);
      return t;
    }();
    return table;
  }

  template <typename ContextT>
  const typename ContextT::function_type* find_builtin(std::string_view name)
  {
    const auto& table = builtin_table<ContextT>();
    const auto  it    = table.find(name);
    return it == table.end() ? nullptr : &it->second;
  }

  } // namespace detail
} // namespace reflex::jinja::expr
