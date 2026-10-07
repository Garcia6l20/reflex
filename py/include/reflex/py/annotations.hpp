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
#include <reflex/py/nanobind.hpp>

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

  /** @brief what a bound function does with the result it returns
   *
   * Only for the cases the return type cannot settle. A `widget*` handing back
   * something borrowed and a `widget*` handing back a fresh allocation are the
   * same type, and nothing in the signature tells them apart.
   *
   * @code
   * [[= py::returns{py::nb::rv_policy::reference}]] widget* find(int id);
   * @endcode
   */
  struct returns
  {
    nb::rv_policy policy;
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

  /** @brief show CPython's cycle collector the Python references a class owns
   *
   * A native member holding a Python object is an edge the collector cannot
   * see, so a cycle running through one is never collected. Annotating any
   * member of a bound class this way gives its Python type `tp_traverse` and
   * `tp_clear`, and makes it a GC type.
   *
   * On a data member, the member is visited and, on a clear, reset without
   * allocating: a bare `nb::object` to `None`, another `nb::object` subclass
   * to a null handle, anything else to `T{}`. It may be an `nb::object` or
   * anything derived from one, a `std::optional` of one, a range of them or a
   * map whose keys or values are one, nested as deep as needed.
   *
   * On a member function, it is called as `f(visitor)` on a const object with a
   * py::gc_visitor, for references a data member cannot express: objects found
   * through `nb::find` on native state, or held deeper than the class itself.
   * It must be noexcept, and visit each reference the object owns exactly
   * once, and nothing it does not own.
   *
   * Only a Python object owning its C++ instance reports or clears anything. A
   * view made under rv_policy::reference or reference_internal, a class-typed
   * member read through its owner or a `T&` return, reports nothing: the
   * references belong to the owner.
   *
   * Annotated members of a base are honoured too, private ones included. The
   * functions are never published as methods. A base bound by hand with
   * `nb::dynamic_attr()` loses nanobind's own traversal of `__dict__`, so a
   * cycle through that dictionary is not collected.
   *
   * @code
   * class holder
   * {
   *   [[= py::traverse]] nb::object callback;
   *   [[= py::traverse]] void visit_models(py::gc_visitor& visit) const noexcept;
   *   [[= py::clear]] void drop_models() noexcept;
   * };
   * @endcode
   */
  struct traverse_t
  {};
  inline constexpr traverse_t traverse{};

  /** @brief a member function run by `tp_clear` to break a cycle
   *
   * Called as `f()` after the annotated data members are reset. It must be
   * noexcept, and the object it runs on stays alive and may still be used
   * afterwards, so it drops references rather than destroying state.
   */
  struct clear_t
  {};
  inline constexpr clear_t clear{};

  /** @brief how a member's name is spelled in Python, when it does not say */
  using naming = caseconv::naming;

} // namespace reflex::py
