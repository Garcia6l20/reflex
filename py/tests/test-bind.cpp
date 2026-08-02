#include <doctest/doctest.h>

// Macros do not come out of a module.
#include <reflex/const_check.hpp>

import reflex.py;
import std;

namespace
{
  namespace py = reflex::py;

  struct widget
  {
    auto named(int count) const -> int;
    auto shadowing(int self) const -> int;
    auto deducing(this widget const& self, int n) -> int;
    auto unnamed(int, int) const -> int;

    static auto free_standing(int self) -> int;
  };

  struct first_base
  {};
  struct second_base
  {};
  struct [[= py::skip]] ignored_base
  {};

  struct one_base : first_base
  {};
  struct two_bases : first_base, second_base
  {};
  struct one_kept : first_base, ignored_base
  {};

  consteval auto names_of(std::meta::info fn, std::size_t arity)
  {
    return py::detail::argument_names(reflex::overload{fn, arity});
  }
} // namespace

TEST_CASE("reflex::py: argument names come from the parameters")
{
  static_assert(names_of(^^widget::named, 1).size() == 1);
  static_assert(std::string_view{names_of(^^widget::named, 1)[0]} == "count");

  // All of them or none: nanobind counts the annotations against the signature.
  static_assert(names_of(^^widget::unnamed, 2).empty());

  // The object is not an argument, so a deducing this member reports only what
  // a call site writes.
  static_assert(names_of(^^widget::deducing, 1).size() == 1);
  static_assert(std::string_view{names_of(^^widget::deducing, 1)[0]} == "n");
}

TEST_CASE("reflex::py: a bound class carries at most one base")
{
  static_assert(py::detail::bindable_base(^^one_base) == ^^first_base);
  static_assert(py::detail::bindable_base(^^first_base) == reflex::meta::null);

  consteval {
    // nanobind's class_ carries exactly one base, and a second is refused
    // rather than dropped: an isinstance against it would fail with nothing to
    // point at.
    REFLEX_CONSTEVAL_THROWS(py::detail::bindable_base(^^two_bases));

    // Unless the others are skipped.
    REFLEX_CONSTEVAL_NOTHROW(py::detail::bindable_base(^^one_kept));
  }
}

TEST_CASE("reflex::py: a parameter named self is rejected")
{
  consteval {
    REFLEX_CONSTEVAL_NOTHROW(names_of(^^widget::named, 1));

    // nanobind names the object `self` on every method it binds, so the
    // signature would read `shadowing(self, self: int)` and the argument would
    // be unreachable by keyword.
    REFLEX_CONSTEVAL_THROWS(names_of(^^widget::shadowing, 1));

    // A deducing this member usually spells its object `self`, and that is the
    // object rather than an argument. Not the rejected case.
    REFLEX_CONSTEVAL_NOTHROW(names_of(^^widget::deducing, 1));

    // A static member has no object for the name to collide with.
    REFLEX_CONSTEVAL_NOTHROW(names_of(^^widget::free_standing, 1));
  }
}
