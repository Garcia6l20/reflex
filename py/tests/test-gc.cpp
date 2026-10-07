#include <doctest/doctest.h>

#include <reflex/const_check.hpp>

import reflex.py;
import std;

namespace
{
  namespace py = reflex::py;

  struct untouched
  {
    int      value;
    unsigned bits : 3;

    template <typename U> auto generic(U u) const -> U
    {
      return u;
    }

    struct nested
    {};
    using alias = int;
  };

  struct hooks
  {
    [[= py::traverse]] void report(py::gc_visitor&) const noexcept;
    [[= py::clear]] void    forget() noexcept;
    void                    ordinary();
  };

  struct derived_hooks : hooks
  {};

  struct const_member
  {
    [[= py::traverse]] int const value = 0;
  };

  struct not_python
  {
    [[= py::traverse]] std::vector<std::string> names;
  };

  struct static_hook
  {
    [[= py::traverse]] static void report(py::gc_visitor&) noexcept;
  };

  struct throwing_traverse
  {
    [[= py::traverse]] void report(py::gc_visitor&) const;
  };

  struct throwing_clear
  {
    [[= py::traverse]] void report(py::gc_visitor&) const noexcept;
    [[= py::clear]] void    forget();
  };

  struct reference_member
  {
    [[= py::traverse]] int& value;
  };

  struct clear_only
  {
    [[= py::clear]] void forget() noexcept;
  };

  consteval auto member(std::meta::info scope, std::string_view name) -> std::meta::info
  {
    return reflex::meta::member_named(scope, name);
  }
}

TEST_CASE("reflex::py: a class without py::traverse gets no GC slots")
{
  static_assert(not py::detail::is_traversed(^^untouched));
}

TEST_CASE("reflex::py: py::traverse on a member function makes a GC type")
{
  static_assert(py::detail::is_traversed(^^hooks));
  static_assert(py::detail::is_traversed(^^derived_hooks));
  static_assert(py::detail::gc_members_of(^^derived_hooks, ^^py::traverse_t).size() == 1);
}

TEST_CASE("reflex::py: GC hooks are not published as methods")
{
  static_assert(py::is_gc_hook(member(^^hooks, "report")));
  static_assert(py::is_gc_hook(member(^^hooks, "forget")));
  static_assert(not py::is_gc_hook(member(^^hooks, "ordinary")));
  static_assert(not py::detail::is_bindable_method(member(^^hooks, "report")));
  static_assert(not py::detail::is_bindable_method(member(^^hooks, "forget")));
}

TEST_CASE("reflex::py: a py::traverse data member holds Python references")
{
  static_assert(not py::detail::holds_python_v<int>);
  static_assert(not py::detail::holds_python_v<std::vector<std::string>>);
  static_assert(not py::detail::holds_python_v<std::map<std::string, int>>);
}

TEST_CASE("reflex::py: misplaced GC annotations are rejected")
{
  consteval {
    REFLEX_CONSTEVAL_THROWS(py::detail::is_traversed(^^const_member));
    REFLEX_CONSTEVAL_THROWS(py::detail::is_traversed(^^not_python));
    REFLEX_CONSTEVAL_THROWS(py::detail::is_traversed(^^static_hook));
    REFLEX_CONSTEVAL_THROWS(py::detail::is_traversed(^^clear_only));
    REFLEX_CONSTEVAL_THROWS(py::detail::is_traversed(^^throwing_traverse));
    REFLEX_CONSTEVAL_THROWS(py::detail::is_traversed(^^throwing_clear));
    REFLEX_CONSTEVAL_THROWS(py::detail::is_traversed(^^reference_member));
  }
}
