/** @file
 * @brief what a class or a member can say about its Python surface
 *
 * @code
 * class widget
 * {
 * public:
 *   [[= py::doc{"how many times it turned"}]] int count() const;
 *   [[= py::rename{"reset_all"}]] void reset();
 *   [[= py::skip]] int internal();
 *   [[= py::readonly]] int serial;
 * };
 * @endcode
 */
#pragma once

#ifndef REFLEX_MODULE
#include <reflex/caseconv.hpp>
#include <reflex/constant.hpp>
#endif

#include <reflex/const_check.hpp>

REFLEX_EXPORT namespace reflex::py
{
  /** @brief keep this out of the Python surface */
  struct skip_t
  {};
  inline constexpr skip_t skip{};

  /** @brief expose this data member read-only
   *
   * A const data member is read-only whether it says so or not, since there is
   * nothing to assign through.
   */
  struct readonly_t
  {};
  inline constexpr readonly_t readonly{};

  /** @brief a Python identifier, or a dunder */
  consteval auto is_python_name(std::string_view name) -> bool
  {
    if(name.empty())
    {
      return false;
    }
    const auto ordinary = [](char c) {
      return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9')
          or c == '_';
    };
    if(name.front() >= '0' and name.front() <= '9')
    {
      return false;
    }
    return std::ranges::all_of(name, ordinary);
  }

  /** @brief the Python name of a member, overriding the naming policy
   *
   * A dunder is a legal spelling here: it is how an operator the mapping table
   * gets wrong, or a conversion it declines to bind, is reached.
   */
  struct rename : constant_string
  {
    consteval rename(std::string_view name) : constant_string{name}
    {
      // A dotted name would be accepted by nanobind and produce an attribute
      // nothing can reach, so the check is worth more than its cost.
      REFLEX_META_CHECK(
          is_python_name(name), "a py::rename must be a Python identifier", ^^rename);
    }
  };

  /** @brief the docstring of a class or a member
   *
   * A `///` comment is not reachable through reflection, so this is the only
   * source of one.
   */
  struct doc : constant_string
  {
    consteval doc(std::string_view text) : constant_string{text}
    {}
  };

  /** @brief publish this nested namespace as a submodule
   *
   * Binding a namespace does not recurse on its own. A nested namespace is
   * usually an implementation detail, and skipping one by convention - anything
   * called `detail` - would be a rule the source does not show.
   */
  struct submodule_t
  {};
  inline constexpr submodule_t submodule{};

  /** @brief how a member's name is spelled in Python, when it does not say */
  using naming = caseconv::naming;

} // namespace reflex::py
