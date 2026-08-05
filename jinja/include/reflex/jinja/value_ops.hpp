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

    static bool equal(const value_type& a, const value_type& b)
    {
      return reflex::visit(
          [&]<typename LHS, typename RHS>(LHS const& lhs, RHS const& rhs) -> bool {
            using DLHS = std::decay_t<LHS>;
            using DRHS = std::decay_t<RHS>;
            if constexpr(eq_comparable_c<DLHS, DRHS>)
            {
              return lhs == rhs;
            }
            else if constexpr(parsable_c<DLHS> and str_c<DRHS>)
            {
              auto parsed = parse<DLHS>(rhs);
              return parsed and (std::move(parsed).value() == lhs);
            }
            else if constexpr(str_c<DLHS> and parsable_c<DRHS>)
            {
              auto parsed = parse<DRHS>(lhs);
              return parsed and (rhs == std::move(parsed).value());
            }
            else
            {
              return false;
            }
          },
          a, b);
    }

    // Returns negative / zero / positive like strcmp. Numeric only: an ordering over anything else
    // would silently change what `a < b` means in a template.
    static int compare(const value_type& a, const value_type& b)
    {
      static const auto to_double = [](const value_type& v) -> double {
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
        throw std::runtime_error("Cannot compare non-numeric values with ordering operators");
      };
      const double la = to_double(a);
      const double rb = to_double(b);
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

    static value_type arith_add(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            return {*la + *rb};
          }
        }
      }
      if(auto* la = std::get_if<std::string>(&a))
      {
        if(auto* rb = std::get_if<std::string>(&b))
        {
          return {*la + *rb};
        }
      }
      return {to_double(a) + to_double(b)};
    }

    static value_type arith_sub(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            return {*la - *rb};
          }
        }
      }
      return {to_double(a) - to_double(b)};
    }

    static value_type arith_mul(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            return {*la * *rb};
          }
        }
      }
      return {to_double(a) * to_double(b)};
    }

    static value_type arith_div(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            if(*rb == 0)
            {
              throw std::runtime_error("Integer division by zero");
            }
            return {*la / *rb};
          }
        }
      }
      double rb = to_double(b);
      if(rb == 0.0)
      {
        throw std::runtime_error("Division by zero");
      }
      return {to_double(a) / rb};
    }

    static value_type arith_mod(const value_type& a, const value_type& b)
    {
      if constexpr(integral_type_info != meta::null)
      {
        if(auto* la = std::get_if<integral_type>(&a))
        {
          if(auto* rb = std::get_if<integral_type>(&b))
          {
            if(*rb == 0)
            {
              throw std::runtime_error("Modulo by zero");
            }
            return {*la % *rb};
          }
        }
      }
      throw std::runtime_error("'%' requires integer operands");
    }

    static double to_double(const value_type& v)
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
      throw std::runtime_error("Expected numeric value");
    }
  };
} // namespace reflex::jinja::expr
