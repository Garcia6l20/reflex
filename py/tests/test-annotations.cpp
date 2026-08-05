#include <doctest/doctest.h>

// Macros do not come out of a module.
#include <reflex/const_check.hpp>

import reflex.py;
import std;

namespace
{
  namespace py = reflex::py;

  struct plain
  {
    int                       value;
    int const                 fixed = 0;
    [[= py::readonly]] int    serial;
    [[= py::rename{"other"}]] int renamed;
    [[= py::skip]] int        hidden;

    [[= py::doc{"turns\nand \"turns\""}]] int spin() const;
    [[= py::skip]] int internal() const;
    int                plain_method() const;

    auto operator+(plain const&) const -> plain;
  };

  struct [[= py::naming::snake_case]] scoped
  {
    int alreadySnake;
    int camelCaseMember;

    [[= py::naming::pascal_case]] int overridden;
    [[= py::rename{"kept"}]] int camelCaseRenamed;

    int camelCaseMethod() const;
  };

  struct undecorated
  {
    int only;
  };

  consteval auto member(std::meta::info scope, std::string_view name) -> std::meta::info
  {
    return reflex::meta::member_named(scope, name);
  }
} // namespace

TEST_CASE("reflex::py: skip")
{
  static_assert(py::is_skipped(member(^^plain, "hidden")));
  static_assert(py::is_skipped(member(^^plain, "internal")));
  static_assert(not py::is_skipped(member(^^plain, "value")));
  static_assert(not py::is_skipped(member(^^plain, "spin")));
  static_assert(not py::is_skipped(^^plain));
}

TEST_CASE("reflex::py: readonly")
{
  static_assert(py::is_readonly(member(^^plain, "serial")));
  static_assert(py::is_readonly(member(^^plain, "fixed")));
  static_assert(not py::is_readonly(member(^^plain, "value")));

  // A method is not a data member, so nothing is read-only about it.
  static_assert(not py::is_readonly(member(^^plain, "spin")));
}

TEST_CASE("reflex::py: python_name")
{
  static_assert(py::python_name(member(^^plain, "value")) == "value");
  static_assert(py::python_name(member(^^plain, "renamed")) == "other");
  static_assert(py::python_name(member(^^plain, "plain_method")) == "plain_method");

  // Unchanged by this step. The operator mapping has this to map from.
  // member_named looks an identifier up, and an operator carries none.
  static_assert(
      py::python_name(reflex::meta::callables_named(^^plain, "operator+").front())
      == "operator+");

  // A constructor has neither an identifier nor an operator to spell.
  static_assert(py::python_name(reflex::meta::constructors_of(^^undecorated).front()) == "");
}

TEST_CASE("reflex::py: python_name follows the enclosing naming policy")
{
  static_assert(py::python_name(member(^^scoped, "alreadySnake")) == "already_snake");
  static_assert(py::python_name(member(^^scoped, "camelCaseMember")) == "camel_case_member");
  static_assert(py::python_name(member(^^scoped, "camelCaseMethod")) == "camel_case_method");

  // The member's own policy beats the scope's.
  static_assert(py::python_name(member(^^scoped, "overridden")) == "Overridden");

  // A rename beats both.
  static_assert(py::python_name(member(^^scoped, "camelCaseRenamed")) == "kept");

  // No policy anywhere leaves the name as written.
  static_assert(py::python_name(member(^^undecorated, "only")) == "only");

  // A naming policy on a scope governs what is inside it, not what it is
  // called. The class carrying the annotation keeps its own name.
  static_assert(py::python_name(^^scoped) == "scoped");
}

TEST_CASE("reflex::py: doc")
{
  static_assert(py::doc_of(member(^^plain, "spin")) == "turns\nand \"turns\"");
  static_assert(py::doc_of(member(^^plain, "plain_method")) == "");
  static_assert(py::doc_of(^^plain) == "");
}

TEST_CASE("reflex::py: a rename must be a Python identifier")
{
  consteval {
    REFLEX_CONSTEVAL_NOTHROW(py::rename{"ok"});
    REFLEX_CONSTEVAL_NOTHROW(py::rename{"_ok"});
    REFLEX_CONSTEVAL_NOTHROW(py::rename{"__add__"});
    REFLEX_CONSTEVAL_NOTHROW(py::rename{"ok2"});

    REFLEX_CONSTEVAL_THROWS(py::rename{""});
    REFLEX_CONSTEVAL_THROWS(py::rename{"a.b"});
    REFLEX_CONSTEVAL_THROWS(py::rename{"has space"});
    REFLEX_CONSTEVAL_THROWS(py::rename{"2fast"});
  }
}
