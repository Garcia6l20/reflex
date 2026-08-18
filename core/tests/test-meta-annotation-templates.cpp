#include <doctest/doctest.h>

#include <reflex/const_check.hpp>

import reflex.core;
import std;

namespace probe
{
  struct mark
  {
    int id;
  };

  struct with_templates
  {
    [[= mark{1}]] int          field;
    int                        bare;
    [[= mark{2}]] void         marked() {}
    void                       plain() {}
    template <typename F> void generic(F&&) {}
    template <typename F> [[= mark{3}]] void marked_generic(F&&) {}
    template <typename T> static constexpr int counter = 0;
    template <typename T> struct nested
    {};
  };

  struct only_a_template
  {
    template <typename F> void generic(F&&) {}
  };

  consteval auto member(std::string_view name) -> std::meta::info
  {
    return reflex::meta::member_named(^^probe::with_templates, name);
  }
} // namespace probe

TEST_CASE("reflex::meta::has_annotation: a template answers no instead of throwing")
{
  using reflex::meta::has_annotation;

  static_assert(has_annotation(probe::member("field"), ^^probe::mark));
  static_assert(has_annotation(probe::member("marked"), ^^probe::mark));

  static_assert(not has_annotation(probe::member("generic"), ^^probe::mark));
  static_assert(not has_annotation(probe::member("counter"), ^^probe::mark));
  static_assert(not has_annotation(probe::member("nested"), ^^probe::mark));

  consteval {
    REFLEX_CONSTEVAL_NOTHROW(has_annotation(probe::member("generic"), ^^probe::mark));
    REFLEX_CONSTEVAL_NOTHROW(has_annotation(probe::member("counter"), ^^probe::mark));
    REFLEX_CONSTEVAL_NOTHROW(has_annotation(probe::member("nested"), ^^probe::mark));
  }
}

TEST_CASE("reflex::meta::member_functions_annotated_with: templates are passed over")
{
  constexpr auto found =
      std::define_static_array(reflex::meta::member_functions_annotated_with(^^probe::with_templates, ^^probe::mark));

  static_assert(found.size() == 1);
  static_assert(found[0] == ^^probe::with_templates::marked);
}

TEST_CASE("reflex::meta::member_functions_annotated_with: nothing but a template")
{
  static_assert(reflex::meta::member_functions_annotated_with(^^probe::only_a_template, ^^probe::mark).empty());
  static_assert(reflex::meta::first_member_function_annotated_with(^^probe::only_a_template, ^^probe::mark)
                == reflex::meta::null);
}

TEST_CASE("reflex::meta::nonstatic_data_members_annotated_with: templates are passed over")
{
  constexpr auto found = std::define_static_array(
      reflex::meta::nonstatic_data_members_annotated_with(^^probe::with_templates, ^^probe::mark));

  static_assert(found.size() == 1);
  static_assert(found[0] == ^^probe::with_templates::field);
}

TEST_CASE("reflex::meta::annotations_of_with: an annotated template is readable once substituted")
{
  constexpr auto instance = std::meta::substitute(probe::member("marked_generic"), {^^int});

  static_assert(not reflex::meta::has_annotation(probe::member("marked_generic"), ^^probe::mark));
  static_assert(reflex::meta::has_annotation(instance, ^^probe::mark));
  static_assert(reflex::meta::annotation_value_of_with<probe::mark>(instance).id == 3);
}

TEST_CASE("reflex::meta::annotation_value_of_with: a template has no such annotation")
{
  consteval {
    REFLEX_CONSTEVAL_THROWS(reflex::meta::annotation_value_of_with<probe::mark>(probe::member("generic")));
    REFLEX_CONSTEVAL_THROWS(reflex::meta::annotation_value_of_with<probe::mark>(probe::member("marked_generic")));
  }
}
