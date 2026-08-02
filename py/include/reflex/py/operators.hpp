/** @file
 * @brief how a C++ operator is spelled in Python
 *
 * A table, not a rule. `operator+` is `__add__` written binary and `__pos__`
 * written unary, `operator<=>` is six names at once, and several operators have
 * no Python counterpart at all.
 */
#pragma once

#ifndef REFLEX_MODULE
#include <array>
#include <string_view>
#endif

REFLEX_EXPORT namespace reflex::py::detail
{
  /** @brief the six comparisons, in the order the spaceship expansion uses */
  inline constexpr std::array<std::string_view, 6> comparison_names{
      "__lt__", "__le__", "__eq__", "__ne__", "__gt__", "__ge__"};

  consteval auto is_spaceship(std::string_view spelling) -> bool
  {
    return spelling == "operator<=>";
  }

  /** @brief does @p spelling mutate its left operand, `+=` and friends
   *
   * Ends in `=` without being a comparison. The result of one has to be the
   * object it mutated, which is what makes its return policy different.
   */
  consteval auto is_in_place(std::string_view spelling) -> bool
  {
    if(not spelling.ends_with("=") or spelling == "operator=")
    {
      return false;
    }
    return spelling != "operator==" and spelling != "operator!=" and spelling != "operator<="
       and spelling != "operator>=" and spelling != "operator<=>";
  }

  /** @brief the Python name of a member operator, empty when it has none
   *
   * @p arity counts the arguments a call site writes, so the object is not one
   * of them. That is what tells a unary `operator-` from a binary one.
   *
   * Left deliberately unbound:
   *  - `operator=`, because assignment in Python rebinds a name
   *  - `operator->` and unary `operator*`, which have no counterpart
   *  - `operator!`, since negation goes through `__bool__`
   *  - `operator++` and `operator--`, which mutate in place with no Python form
   *  - `operator&&`, `operator||` and `operator,`, which do not translate
   *
   * A py::rename reaches any of these, which is what it accepts a dunder for.
   */
  consteval auto python_operator(std::string_view spelling, std::size_t arity)
      -> std::string_view
  {
    const bool binary = arity == 1;

    if(spelling == "operator+")
    {
      return binary ? "__add__" : "__pos__";
    }
    if(spelling == "operator-")
    {
      return binary ? "__sub__" : "__neg__";
    }
    if(spelling == "operator*")
    {
      // Unary is a dereference and stays unbound.
      return binary ? "__mul__" : "";
    }
    if(spelling == "operator~")
    {
      return arity == 0 ? "__invert__" : "";
    }
    // A call takes whatever it takes.
    if(spelling == "operator()")
    {
      return "__call__";
    }
    // Everything below is written with one right-hand operand.
    if(not binary)
    {
      return "";
    }

    if(spelling == "operator/")
    {
      return "__truediv__";
    }
    if(spelling == "operator%")
    {
      return "__mod__";
    }
    if(spelling == "operator&")
    {
      return "__and__";
    }
    if(spelling == "operator|")
    {
      return "__or__";
    }
    if(spelling == "operator^")
    {
      return "__xor__";
    }
    if(spelling == "operator<<")
    {
      return "__lshift__";
    }
    if(spelling == "operator>>")
    {
      return "__rshift__";
    }

    if(spelling == "operator==")
    {
      return "__eq__";
    }
    if(spelling == "operator!=")
    {
      return "__ne__";
    }
    if(spelling == "operator<")
    {
      return "__lt__";
    }
    if(spelling == "operator<=")
    {
      return "__le__";
    }
    if(spelling == "operator>")
    {
      return "__gt__";
    }
    if(spelling == "operator>=")
    {
      return "__ge__";
    }

    if(spelling == "operator+=")
    {
      return "__iadd__";
    }
    if(spelling == "operator-=")
    {
      return "__isub__";
    }
    if(spelling == "operator*=")
    {
      return "__imul__";
    }
    if(spelling == "operator/=")
    {
      return "__itruediv__";
    }
    if(spelling == "operator%=")
    {
      return "__imod__";
    }
    if(spelling == "operator&=")
    {
      return "__iand__";
    }
    if(spelling == "operator|=")
    {
      return "__ior__";
    }
    if(spelling == "operator^=")
    {
      return "__ixor__";
    }
    if(spelling == "operator<<=")
    {
      return "__ilshift__";
    }
    if(spelling == "operator>>=")
    {
      return "__irshift__";
    }

    if(spelling == "operator[]")
    {
      return "__getitem__";
    }
    if(spelling == "operator()")
    {
      return "__call__";
    }
    return "";
  }

} // namespace reflex::py::detail
