#include <doctest/doctest.h>

#include <reflex/const_check.hpp>

import reflex.core;
import std;

namespace
{
  // An annotation type has to be structural, so no std::string_view member.
  struct label
  {
    int weight;
  };

  enum class mode
  {
    fast,
    small
  };

  struct subject
  {
    [[= label{3}]] int       titled;
    [[= mode::small]] int    switched;
    [[= 42]] int             counted;
    int                      bare;
  };

  consteval auto member(std::string_view name) -> std::meta::info
  {
    return reflex::meta::member_named(^^subject, name);
  }
} // namespace

TEST_CASE("reflex::meta::annotation_value_of_with: a class type")
{
  using reflex::meta::annotation_value_of_with;

  static_assert(annotation_value_of_with<label>(member("titled")).weight == 3);

  // By reference, so reading one costs no copy.
  static_assert(
      std::is_reference_v<decltype(annotation_value_of_with<label>(member("titled")))>);
}

TEST_CASE("reflex::meta::annotation_value_of_with: a value that is not an object")
{
  using reflex::meta::annotation_value_of_with;

  // An enumeration or an arithmetic annotation is a value, not an object, so
  // there is nothing for a reference to bind to and it comes back by value.
  static_assert(annotation_value_of_with<mode>(member("switched")) == mode::small);
  static_assert(not std::is_reference_v<decltype(annotation_value_of_with<mode>(
                    member("switched")))>);

  static_assert(annotation_value_of_with<int>(member("counted")) == 42);
}

TEST_CASE("reflex::meta::annotation_value_of_with: a missing annotation is rejected")
{
  consteval {
    REFLEX_CONSTEVAL_NOTHROW(reflex::meta::annotation_value_of_with<mode>(member("switched")));
    REFLEX_CONSTEVAL_THROWS(reflex::meta::annotation_value_of_with<mode>(member("bare")));
    REFLEX_CONSTEVAL_THROWS(reflex::meta::annotation_value_of_with<label>(member("switched")));
  }
}
