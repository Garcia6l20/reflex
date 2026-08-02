/** @file
 * @brief reading a member's Python surface off its annotations
 *
 * Every question the binder asks about a reflection before emitting anything
 * is answered here, and nowhere else.
 */
#pragma once

#ifndef REFLEX_MODULE
#include <meta>

#include <reflex/meta.hpp>
#endif

#include <reflex/py/annotations.hpp>

REFLEX_EXPORT namespace reflex::py
{
  /** @brief is @p r kept out of the Python surface */
  consteval auto is_skipped(std::meta::info r) -> bool
  {
    return meta::has_annotation(r, ^^skip_t);
  }

  /** @brief is @p r exposed without a setter
   *
   * A const data member has no setter to expose, so the annotation is only one
   * of the two ways to get here.
   */
  consteval auto is_readonly(std::meta::info r) -> bool
  {
    if(meta::has_annotation(r, ^^readonly_t))
    {
      return true;
    }
    return std::meta::is_nonstatic_data_member(r)
       and std::meta::is_const_type(std::meta::type_of(r));
  }

  /** @brief the Python name of @p r
   *
   * A py::rename wins outright. Failing that, a py::naming on @p r, then one on
   * the scope enclosing it, then the name as written.
   *
   * spelling_of rather than identifier_of, so an operator has a name here too
   * and the operator mapping has something to map from. Empty for anything with
   * neither an identifier nor an operator to spell, a constructor among them.
   */
  consteval auto python_name(std::meta::info r) -> constant_string
  {
    if(auto renamed = meta::annotations_of_with(r, ^^rename); not renamed.empty())
    {
      return extract<rename>(renamed.front());
    }

    const auto written = meta::spelling_of(r);
    if(written.empty())
    {
      return constant_string{""};
    }

    // A naming policy on a scope governs what is inside it, not what it is
    // called. Without this a `[[= py::naming::pascal_case]]` meant to respell a
    // class's members respells the class as well.
    if(not std::meta::is_type(r) and not std::meta::is_namespace(r))
    {
      if(auto n = meta::annotations_of_with(r, ^^naming); not n.empty())
      {
        return caseconv::to_case(written, extract<naming>(n.front()));
      }
    }
    if(auto n = meta::annotations_of_with(std::meta::parent_of(r), ^^naming); not n.empty())
    {
      return caseconv::to_case(written, extract<naming>(n.front()));
    }
    return written;
  }

  /** @brief the docstring of @p r, empty when it carries none */
  consteval auto doc_of(std::meta::info r) -> constant_string
  {
    if(auto d = meta::annotations_of_with(r, ^^doc); not d.empty())
    {
      return extract<doc>(d.front());
    }
    return constant_string{""};
  }

} // namespace reflex::py
