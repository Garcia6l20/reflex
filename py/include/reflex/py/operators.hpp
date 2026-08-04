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
  /** @brief how many operands a call site writes, the object aside */
  enum class shape : unsigned char
  {
    unary,  ///< no argument, `-a`
    binary, ///< one argument, `a - b`
    any     ///< whatever it takes, `operator()`
  };

  struct operator_mapping
  {
    std::string_view spelling;
    std::string_view dunder;
    shape            takes;
    bool             in_place = false;
  };

  /** @brief every operator with a Python name, and nothing else
   *
   * One row per (spelling, shape) pair, so the mapping, the arity it holds for
   * and whether it mutates its left operand are one fact rather than three
   * encodings that can drift.
   *
   * Absent, and therefore unbound:
   *  - `operator=`, because assignment in Python rebinds a name
   *  - `operator->` and unary `operator*`, which have no counterpart
   *  - `operator!`, since negation goes through `__bool__`
   *  - `operator++` and `operator--`, which mutate in place with no Python form
   *  - `operator&&`, `operator||` and `operator,`, which do not translate
   *
   * A py::rename reaches any of these, which is what it accepts a dunder for.
   */
  inline constexpr auto operator_table = std::to_array<operator_mapping>({
      {"operator+", "__pos__", shape::unary},
      {"operator-", "__neg__", shape::unary},
      {"operator~", "__invert__", shape::unary},

      {"operator+", "__add__", shape::binary},
      {"operator-", "__sub__", shape::binary},
      {"operator*", "__mul__", shape::binary},
      {"operator/", "__truediv__", shape::binary},
      {"operator%", "__mod__", shape::binary},
      {"operator&", "__and__", shape::binary},
      {"operator|", "__or__", shape::binary},
      {"operator^", "__xor__", shape::binary},
      {"operator<<", "__lshift__", shape::binary},
      {"operator>>", "__rshift__", shape::binary},

      {"operator<", "__lt__", shape::binary},
      {"operator<=", "__le__", shape::binary},
      {"operator==", "__eq__", shape::binary},
      {"operator!=", "__ne__", shape::binary},
      {"operator>", "__gt__", shape::binary},
      {"operator>=", "__ge__", shape::binary},

      {"operator+=", "__iadd__", shape::binary, true},
      {"operator-=", "__isub__", shape::binary, true},
      {"operator*=", "__imul__", shape::binary, true},
      {"operator/=", "__itruediv__", shape::binary, true},
      {"operator%=", "__imod__", shape::binary, true},
      {"operator&=", "__iand__", shape::binary, true},
      {"operator|=", "__ior__", shape::binary, true},
      {"operator^=", "__ixor__", shape::binary, true},
      {"operator<<=", "__ilshift__", shape::binary, true},
      {"operator>>=", "__irshift__", shape::binary, true},

      {"operator[]", "__getitem__", shape::binary},
      {"operator()", "__call__", shape::any},
  });

  constexpr auto matches(operator_mapping row, std::size_t arity) -> bool
  {
    switch(row.takes)
    {
      case shape::any:
        return true;
      case shape::unary:
        return arity == 0;
      case shape::binary:
        return arity == 1;
    }
    return false;
  }

  /** @brief the Python name of a member operator, empty when it has none
   *
   * @p arity counts the arguments a call site writes, so the object is not one
   * of them. That is what tells a unary `operator-` from a binary one.
   */
  constexpr auto python_operator(std::string_view spelling, std::size_t arity)
      -> std::string_view
  {
    for(auto row : operator_table)
    {
      if(row.spelling == spelling and matches(row, arity))
      {
        return row.dunder;
      }
    }
    return {};
  }

  /** @brief does @p spelling mutate its left operand, `+=` and friends
   *
   * Read off the table rather than off the spelling. Ends-in-`=` needs a
   * hand-kept list of the comparisons it would otherwise catch, and that list is
   * a second place to forget an operator.
   */
  constexpr auto is_in_place(std::string_view spelling) -> bool
  {
    for(auto row : operator_table)
    {
      if(row.spelling == spelling and row.in_place)
      {
        return true;
      }
    }
    return false;
  }

  consteval auto is_spaceship(std::string_view spelling) -> bool
  {
    return spelling == "operator<=>";
  }

  /** @brief the comparisons a spaceship expands into, in expansion order */
  inline constexpr std::array<std::string_view, 6> comparison_spellings{
      "operator<", "operator<=", "operator==", "operator!=", "operator>", "operator>="};

  /** @brief the same six as dunders, read off the table so there is one source */
  inline constexpr std::array<std::string_view, 6> comparison_names = [] {
    std::array<std::string_view, 6> names{};
    for(std::size_t i = 0; i < names.size(); ++i)
    {
      names[i] = python_operator(comparison_spellings[i], 1);
    }
    return names;
  }();

} // namespace reflex::py::detail
