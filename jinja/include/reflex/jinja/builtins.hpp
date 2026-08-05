#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/caseconv.hpp>
#include <reflex/exception.hpp>
#include <reflex/utils.hpp>

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

  // range() materializes its whole array, so it refuses a size that would die in the allocator
  // instead of a template author's typo.
  inline constexpr std::int64_t _max_range_elements = 1'000'000;

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

  // The integer argument of a filter that counts or indexes. Guarded on the integral alternative
  // existing at all: a context whose value type has none must still compile.
  template <typename ValueT>
  std::int64_t int_arg(std::string_view name, const ValueT& v, std::string_view what)
  {
    using ops     = value_ops<ValueT>;
    const auto& d = deref(v);
    if constexpr(ops::integral_type_info != meta::null)
    {
      if(auto* i = std::get_if<typename ops::integral_type>(&d))
      {
        return static_cast<std::int64_t>(*i);
      }
    }
    builtin_error(name, std::format("{} must be an integer", what));
  }

  // Renders one value through a format spec, the way {{ }} does. An aggregate bound by reference
  // is not formattable, so the guard is a compile-time branch and the failure a runtime throw.
  template <typename ValueT>
  std::string format_value(std::string_view name, const ValueT& v, std::string_view spec)
  {
    return reflex::visit(
        [&]<typename T>(T&& x) -> std::string {
          using U = std::decay_t<T>;
          if constexpr(std::formattable<U, char>)
          {
            return std::vformat(spec, std::make_format_args(x));
          }
          else
          {
            builtin_error(
                name,
                std::format("value of type {} is not formattable", display_string_of(dealias(^^U))));
          }
        },
        v);
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
    t.emplace("length", length);
    t.emplace("count", std::move(length));

    // default(v, fallback) - the null-coalescing filter. Tests null, not falsiness: `x or fallback`
    // already covers the falsy case, and two meanings behind one name is the trap.
    t.emplace("default", [](std::span<const value_type> args) -> value_type {
      check_arity("default", args, 2, 2);
      return deref(args[0]).is_null() ? args[1] : args[0];
    });

    // join(seq, sep = "") - each element rendered the way {{ }} renders it.
    t.emplace("join", [](std::span<const value_type> args) -> value_type {
      check_arity("join", args, 1, 2);
      const auto* a = as_array(args[0]);
      if(a == nullptr)
      {
        builtin_error("join", "argument 1 must be an array");
      }
      const std::string sep = args.size() == 2 ? string_arg("join", args[1], "separator") : "";
      std::string       out;
      bool              first = true;
      for(const auto& elem : *a)
      {
        if(not first)
        {
          out += sep;
        }
        first = false;
        out += format_value("join", elem, "{}");
      }
      return out;
    });

    // reverse(seq) - a reversed copy. A string reverses bytes, not code points.
    t.emplace("reverse", [](std::span<const value_type> args) -> value_type {
      check_arity("reverse", args, 1, 1);
      if(auto* s = as_string(args[0]))
      {
        return std::string{s->rbegin(), s->rend()};
      }
      if(auto* a = as_array(args[0]))
      {
        return *a | std::views::reverse | std::ranges::to<typename ContextT::array_type>();
      }
      builtin_error("reverse", "expects a string or an array");
    });

    // range(n) / range(lo, hi) / range(lo, hi, step) - the only way to loop a fixed count.
    t.emplace("range", [](std::span<const value_type> args) -> value_type {
      check_arity("range", args, 1, 3);
      std::int64_t lo = 0;
      std::int64_t hi = 0;
      std::int64_t step = 1;
      if(args.size() == 1)
      {
        hi = int_arg("range", args[0], "stop");
      }
      else
      {
        lo = int_arg("range", args[0], "start");
        hi = int_arg("range", args[1], "stop");
        if(args.size() == 3)
        {
          step = int_arg("range", args[2], "step");
        }
      }
      if(step == 0)
      {
        builtin_error("range", "step must not be zero");
      }
      const std::int64_t span  = step > 0 ? hi - lo : lo - hi;
      const std::int64_t mag   = step > 0 ? step : -step;
      const std::int64_t count = span > 0 ? (span + mag - 1) / mag : 0;
      if(count > _max_range_elements)
      {
        builtin_error(
            "range", std::format("refuses to build more than {} elements", _max_range_elements));
      }
      typename ContextT::array_type out;
      out.reserve(static_cast<std::size_t>(count));
      for(std::int64_t i = 0, v = lo; i < count; ++i, v += step)
      {
        out.emplace_back(int(v));
      }
      return out;
    });

    // format(v, spec = "{}") - std::format from inside a template.
    t.emplace("format", [](std::span<const value_type> args) -> value_type {
      check_arity("format", args, 1, 2);
      const std::string spec = args.size() == 2 ? string_arg("format", args[1], "spec") : "{}";
      return format_value("format", args[0], spec);
    });

    // tojson(v) - the serde::json encoding of the value, which already knows poly::var.
    t.emplace("tojson", [](std::span<const value_type> args) -> value_type {
      check_arity("tojson", args, 1, 1);
      std::string             out;
      serde::json::serializer ser{out};
      ser.dump(args[0]);
      return out;
    });
  }

  template <typename ContextT> void register_strings(builtin_table_type<ContextT>& t)
  {
    using value_type = typename ContextT::value_type;
    using array_type = typename ContextT::array_type;

    // ASCII only, through the locale-free helpers in reflex/utils.hpp. A UTF-8 multibyte sequence
    // passes through case conversion unchanged, which is the safe failure.
    t.emplace("upper", [](std::span<const value_type> args) -> value_type {
      check_arity("upper", args, 1, 1);
      return caseconv::to_upper(string_arg("upper", args[0], "argument 1"));
    });

    t.emplace("lower", [](std::span<const value_type> args) -> value_type {
      check_arity("lower", args, 1, 1);
      return caseconv::to_lower(string_arg("lower", args[0], "argument 1"));
    });

    // capitalize(s) - first byte upper, the rest lower, like Jinja.
    t.emplace("capitalize", [](std::span<const value_type> args) -> value_type {
      check_arity("capitalize", args, 1, 1);
      std::string out = caseconv::to_lower(string_arg("capitalize", args[0], "argument 1"));
      if(not out.empty())
      {
        out[0] = char(reflex::to_upper(out[0]));
      }
      return out;
    });

    // trim(s, chars = whitespace) - both ends. The one-argument form delegates to reflex::trim so
    // the definition of whitespace stays single-sourced.
    t.emplace("trim", [](std::span<const value_type> args) -> value_type {
      check_arity("trim", args, 1, 2);
      const std::string& s = string_arg("trim", args[0], "argument 1");
      if(args.size() == 1)
      {
        return std::string{reflex::trim(s)};
      }
      const std::string& chars = string_arg("trim", args[1], "character set");
      std::string_view   view{s};
      while(not view.empty() and chars.find(view.front()) != std::string::npos)
      {
        view.remove_prefix(1);
      }
      while(not view.empty() and chars.find(view.back()) != std::string::npos)
      {
        view.remove_suffix(1);
      }
      return std::string{view};
    });

    // replace(s, from, to) - every occurrence. An empty `from` would loop forever.
    t.emplace("replace", [](std::span<const value_type> args) -> value_type {
      check_arity("replace", args, 3, 3);
      const std::string& s    = string_arg("replace", args[0], "argument 1");
      const std::string& from = string_arg("replace", args[1], "pattern");
      const std::string& to   = string_arg("replace", args[2], "replacement");
      if(from.empty())
      {
        builtin_error("replace", "pattern must not be empty");
      }
      std::string out;
      std::size_t pos = 0;
      while(true)
      {
        const auto hit = s.find(from, pos);
        if(hit == std::string::npos)
        {
          out.append(s, pos, std::string::npos);
          return out;
        }
        out.append(s, pos, hit - pos);
        out += to;
        pos = hit + from.size();
      }
    });

    // split(s) splits on runs of whitespace and drops empty fields, like Python's str.split().
    // split(s, sep) splits on the exact separator and keeps them, so "a,,b" yields three fields.
    t.emplace("split", [](std::span<const value_type> args) -> value_type {
      check_arity("split", args, 1, 2);
      const std::string& s = string_arg("split", args[0], "argument 1");
      array_type         out;
      if(args.size() == 1)
      {
        std::size_t i = 0;
        while(i < s.size())
        {
          while(i < s.size() and is_space(s[i]))
          {
            ++i;
          }
          const std::size_t start = i;
          while(i < s.size() and not is_space(s[i]))
          {
            ++i;
          }
          if(i > start)
          {
            out.emplace_back(s.substr(start, i - start));
          }
        }
        return out;
      }
      const std::string& sep = string_arg("split", args[1], "separator");
      if(sep.empty())
      {
        builtin_error("split", "separator must not be empty");
      }
      std::size_t pos = 0;
      while(true)
      {
        const auto hit = s.find(sep, pos);
        if(hit == std::string::npos)
        {
          out.emplace_back(s.substr(pos));
          return out;
        }
        out.emplace_back(s.substr(pos, hit - pos));
        pos = hit + sep.size();
      }
    });

    t.emplace("startswith", [](std::span<const value_type> args) -> value_type {
      check_arity("startswith", args, 2, 2);
      const std::string& s = string_arg("startswith", args[0], "argument 1");
      return s.starts_with(string_arg("startswith", args[1], "prefix"));
    });

    t.emplace("endswith", [](std::span<const value_type> args) -> value_type {
      check_arity("endswith", args, 2, 2);
      const std::string& s = string_arg("endswith", args[0], "argument 1");
      return s.ends_with(string_arg("endswith", args[1], "suffix"));
    });

    // indent(s, n, first = false) - Jinja's semantics: the first line is left alone by default,
    // because the filter is nearly always used after text already sitting at the target column.
    // A blank line is not indented either, so no line gains trailing whitespace.
    t.emplace("indent", [](std::span<const value_type> args) -> value_type {
      check_arity("indent", args, 2, 3);
      const std::string& s = string_arg("indent", args[0], "argument 1");
      const auto         n = int_arg("indent", args[1], "width");
      if(n < 0)
      {
        builtin_error("indent", "width must not be negative");
      }
      bool first = false;
      if(args.size() == 3)
      {
        const auto* b = std::get_if<bool>(&deref(args[2]));
        if(b == nullptr)
        {
          builtin_error("indent", "first must be a boolean");
        }
        first = *b;
      }
      const auto  width = static_cast<std::size_t>(n);
      std::string out;
      bool        at_line_start = first;
      for(char c : s)
      {
        if(at_line_start and c != '\n')
        {
          out.append(width, ' ');
        }
        at_line_start = (c == '\n');
        out += c;
      }
      return out;
    });

    // truncate(s, n, end = "...") - `n` is the total length of the result, the ellipsis included.
    t.emplace("truncate", [](std::span<const value_type> args) -> value_type {
      check_arity("truncate", args, 2, 3);
      const std::string& s = string_arg("truncate", args[0], "argument 1");
      const auto         n = int_arg("truncate", args[1], "length");
      if(n < 0)
      {
        builtin_error("truncate", "length must not be negative");
      }
      const std::string end =
          args.size() == 3 ? string_arg("truncate", args[2], "ellipsis") : "...";
      const auto limit = static_cast<std::size_t>(n);
      if(s.size() <= limit)
      {
        return s;
      }
      if(limit < end.size())
      {
        builtin_error("truncate", "length is shorter than the ellipsis");
      }
      return s.substr(0, limit - end.size()) + end;
    });
  }

  template <typename ContextT> void register_builtins(builtin_table_type<ContextT>& t)
  {
    register_tier1<ContextT>(t);
    register_strings<ContextT>(t);
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
