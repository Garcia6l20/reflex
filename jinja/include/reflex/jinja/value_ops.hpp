#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/concepts.hpp>
#include <reflex/exception.hpp>
#include <reflex/meta.hpp>
#include <reflex/parse.hpp>
#include <reflex/poly/var.hpp>
#include <reflex/visit.hpp>
#endif

REFLEX_EXPORT namespace reflex::jinja::expr
{
  // Coercions shared by the expression evaluator and the builtin functions. Keeping one copy is
  // what keeps `1 + 2` and `sum([1, 2])` promoting the same way.
  template <typename ValueT> struct value_ops
  {
    using value_type = ValueT;

    static consteval std::meta::info __integral_type()
    {
      if(has_template_arguments(dealias(^^value_type)))
      {
        for(auto t_arg : template_arguments_of(dealias(^^value_type)))
        {
          if(is_int_number_type(t_arg))
          {
            return t_arg;
          }
        }
      }
      // lookup in bases
      for(auto base : bases_of(dealias(^^value_type), std::meta::access_context::current()))
      {
        if(has_template_arguments(dealias(^^base)))
        {
          for(auto t_arg : template_arguments_of(type_of(base)))
          {
            if(is_int_number_type(t_arg))
            {
              return t_arg;
            }
          }
        }
      }
      return meta::null;
    }
    static constexpr auto integral_type_info = __integral_type();
    using integral_type                      = typename[:integral_type_info:];

    static value_type coerce_bool(const value_type& v)
    {
      return reflex::visit(
          [&]<typename T>(T const& value) -> value_type {
            if constexpr(requires { static_cast<bool>(value); })
            {
              return static_cast<bool>(value);
            }
            else if constexpr(requires {
                                { value.empty() } -> std::same_as<bool>;
                              })
            {
              return !value.empty();
            }
            else
            {
              return false;
            }
          },
          v);
    }

    static const value_type& resolve(const value_type& v)
    {
      const value_type* p = &v;
      if constexpr(value_type::template can_hold<value_type&>())
      {
        while(auto inner = p->template get<value_type&>())
        {
          p = &*inner;
        }
      }
      return *p;
    }

    template <typename T> static const T* alternative_of(const value_type& v)
    {
      if constexpr(value_type::template can_hold<T>())
      {
        if(auto held = v.template get<T>())
        {
          return &*held;
        }
      }
      if constexpr(value_type::template can_hold<T&>())
      {
        if(auto held = v.template get<T&>())
        {
          return &*held;
        }
      }
      return nullptr;
    }

    static std::optional<double> number_of(const value_type& v)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* i = alternative_of<integral_type>(v))
        {
          return static_cast<double>(*i);
        }
      }
      if(auto* d = alternative_of<double>(v))
      {
        return *d;
      }
      if(auto* b = alternative_of<bool>(v))
      {
        return static_cast<double>(*b);
      }
      return std::nullopt;
    }

    static const std::string* string_of(const value_type& v)
    {
      return alternative_of<std::string>(v);
    }

    template <typename T> static bool parses_to(std::string_view text, const T& value)
    {
      auto parsed = parse<T>(text);
      return parsed and (std::move(parsed).value() == value);
    }

    static bool equal_text(std::string_view lhs, const value_type& b)
    {
      if(auto* rhs = string_of(b))
      {
        return lhs == *rhs;
      }
      if(auto* rhs = alternative_of<bool>(b))
      {
        return parses_to(lhs, *rhs);
      }
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* rhs = alternative_of<integral_type>(b))
        {
          return parses_to(lhs, *rhs);
        }
      }
      if(auto* rhs = alternative_of<double>(b))
      {
        return parses_to(lhs, *rhs);
      }
      return false;
    }

    static bool equal(const value_type& a, const value_type& b)
    {
      const value_type& rb = resolve(b);
      return reflex::visit(
          [&]<typename LHS>(LHS const& lhs) -> bool {
            using DLHS = std::decay_t<LHS>;
            if(auto* rhs = alternative_of<DLHS>(rb))
            {
              if constexpr(eq_comparable_c<DLHS, DLHS>)
              {
                return lhs == *rhs;
              }
              else
              {
                return false;
              }
            }
            if constexpr(std::is_arithmetic_v<DLHS>)
            {
              if(auto rhs = number_of(rb))
              {
                return static_cast<double>(lhs) == *rhs;
              }
              if(auto* rhs = string_of(rb))
              {
                return parses_to(*rhs, lhs);
              }
              return false;
            }
            else if constexpr(str_c<DLHS>)
            {
              return equal_text(lhs, rb);
            }
            else if constexpr(parsable_c<DLHS>)
            {
              if(auto* rhs = string_of(rb))
              {
                return parses_to(*rhs, lhs);
              }
              return false;
            }
            else
            {
              return false;
            }
          },
          a);
    }

    // Returns negative / zero / positive like strcmp. Numeric only: an ordering over anything else
    // would silently change what `a < b` means in a template.
    static int compare(const value_type& a, const value_type& b)
    {
      constexpr auto what = "Cannot compare non-numeric values with ordering operators";
      const double   la   = to_double(a, what);
      const double   rb   = to_double(b, what);
      if(la < rb)
      {
        return -1;
      }
      if(la > rb)
      {
        return 1;
      }
      return 0;
    }

    // Applies `int_op` when both operands hold the integral alternative, `fallback` otherwise. One
    // copy of that dispatch is what keeps every operator promoting the same way.
    static value_type
        arith_binary(const value_type& a, const value_type& b, auto int_op, auto fallback)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            return int_op(*la, *rb);
          }
        }
      }
      return fallback(a, b);
    }

    static value_type arith_add(const value_type& a, const value_type& b)
    {
      return arith_binary(
          a, b, [](auto l, auto r) -> value_type { return {l + r}; },
          [](const value_type& l, const value_type& r) -> value_type {
            if(auto* ls = std::get_if<std::string>(&l))
            {
              if(auto* rs = std::get_if<std::string>(&r))
              {
                return {*ls + *rs};
              }
            }
            return {to_double(l) + to_double(r)};
          });
    }

    static value_type arith_sub(const value_type& a, const value_type& b)
    {
      return arith_binary(
          a, b, [](auto l, auto r) -> value_type { return {l - r}; },
          [](const value_type& l, const value_type& r) -> value_type {
            return {to_double(l) - to_double(r)};
          });
    }

    static value_type arith_mul(const value_type& a, const value_type& b)
    {
      return arith_binary(
          a, b, [](auto l, auto r) -> value_type { return {l * r}; },
          [](const value_type& l, const value_type& r) -> value_type {
            return {to_double(l) * to_double(r)};
          });
    }

    static value_type arith_div(const value_type& a, const value_type& b)
    {
      return arith_binary(
          a, b,
          [](auto l, auto r) -> value_type {
            if(r == 0)
            {
              throw std::runtime_error("Integer division by zero");
            }
            return {l / r};
          },
          [](const value_type& l, const value_type& r) -> value_type {
            const double rd = to_double(r);
            if(rd == 0.0)
            {
              throw std::runtime_error("Division by zero");
            }
            return {to_double(l) / rd};
          });
    }

    static value_type arith_mod(const value_type& a, const value_type& b)
    {
      return arith_binary(
          a, b,
          [](auto l, auto r) -> value_type {
            if(r == 0)
            {
              throw std::runtime_error("Modulo by zero");
            }
            return {l % r};
          },
          [](const value_type&, const value_type&) -> value_type {
            throw std::runtime_error("'%' requires integer operands");
          });
    }

    static double to_double(const value_type& v, std::string_view what = "Expected numeric value")
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* i = std::get_if<integral_type>(&v))
        {
          return static_cast<double>(*i);
        }
      }
      if(auto* d = std::get_if<double>(&v))
      {
        return *d;
      }
      throw std::runtime_error(std::string(what));
    }
  };
} // namespace reflex::jinja::expr
