#include <doctest/doctest.h>
#include <reflex/const_check.hpp>

import reflex.serde;

import std;

using namespace reflex;
using namespace reflex::serde;

namespace
{
  struct plain
  {
    std::optional<int>         opt;
    std::vector<int>           seq;
    std::string                text;
    std::map<std::string, int> table;
    int                        scalar;
  };

  struct annotated_members
  {
    [[= serde::omit_if_empty{}]] std::optional<int>         opt;
    [[= serde::omit_if_empty{}]] std::vector<int>           seq;
    [[= serde::omit_if_empty{}]] std::string                text;
    [[= serde::omit_if_empty{}]] std::map<std::string, int> table;
    [[= serde::omit_if_empty{}]] std::array<char, 4>        fixed;
    int                                                     scalar;
  };

  struct[[= serde::omit_if_empty{}]] annotated_type
  {
    std::optional<int> opt;
    std::vector<int>   seq;
    int                scalar;
  };

  struct[[= serde::omit_if_empty{^^std::optional}]] filtered_to_optional
  {
    std::optional<int> opt;
    std::vector<int>   seq;
    std::string        text;
  };

  struct[[= serde::omit_if_empty{^^std::string}]] filtered_to_string
  {
    std::optional<int> opt;
    std::string        text;
  };

  struct[[= serde::omit_if_empty{^^std::vector, ^^std::map}]] filtered_to_containers
  {
    std::vector<int>           seq;
    std::map<std::string, int> table;
    std::string                text;
  };

  struct[[= serde::omit_if_empty{}]] cancelled
  {
    [[= serde::no_omit]] std::optional<int> opt;
    std::optional<int>                      other;
  };

  struct bad_scalar
  {
    [[= serde::omit_if_empty{}]] int scalar;
  };

  struct bad_fixed_array
  {
    [[= serde::omit_if_empty{}]] std::array<int, 3> fixed;
  };

  struct bad_filter
  {
    [[= serde::omit_if_empty{^^std::vector}]] std::string text;
  };

  struct[[= serde::no_omit]] no_omit_on_type
  {
    std::optional<int> opt;
  };

  template <typename T> consteval bool omits(std::size_t index)
  {
    return omits_when_empty(
        nonstatic_data_members_of(^^T, std::meta::access_context::current())[index]);
  }

  static_assert(not omits<plain>(0));
  static_assert(not omits<plain>(1));
  static_assert(not omits<plain>(2));
  static_assert(not omits<plain>(3));
  static_assert(not omits<plain>(4));

  static_assert(omits<annotated_members>(0));
  static_assert(omits<annotated_members>(1));
  static_assert(omits<annotated_members>(2));
  static_assert(omits<annotated_members>(3));
  static_assert(omits<annotated_members>(4));
  static_assert(not omits<annotated_members>(5));

  static_assert(omits<annotated_type>(0));
  static_assert(omits<annotated_type>(1));
  static_assert(not omits<annotated_type>(2));

  static_assert(omits<filtered_to_optional>(0));
  static_assert(not omits<filtered_to_optional>(1));
  static_assert(not omits<filtered_to_optional>(2));

  static_assert(not omits<filtered_to_string>(0));
  static_assert(omits<filtered_to_string>(1));

  static_assert(omits<filtered_to_containers>(0));
  static_assert(omits<filtered_to_containers>(1));
  static_assert(not omits<filtered_to_containers>(2));

  static_assert(not omits<cancelled>(0));
  static_assert(omits<cancelled>(1));

  consteval {
    REFLEX_CONSTEVAL_THROWS_WITH("can never be empty", omits<bad_scalar>(0));
    REFLEX_CONSTEVAL_THROWS_WITH("can never be empty", omits<bad_fixed_array>(0));
    REFLEX_CONSTEVAL_THROWS_WITH("lists no type matching", omits<bad_filter>(0));
    REFLEX_CONSTEVAL_THROWS_WITH("belongs on a member", omits<no_omit_on_type>(0));
    REFLEX_CONSTEVAL_THROWS_WITH("a type or a template", omit_if_empty{^^::});
    REFLEX_CONSTEVAL_NOTHROW(omit_if_empty{^^std::optional, ^^std::string});
  }
}

TEST_CASE("an empty value is recognised whatever shape it takes")
{
  CHECK(is_empty_value(std::optional<int>{}));
  CHECK_FALSE(is_empty_value(std::optional<int>{0}));

  CHECK(is_empty_value(std::vector<int>{}));
  CHECK_FALSE(is_empty_value(std::vector<int>{0}));

  CHECK(is_empty_value(std::string{}));
  CHECK_FALSE(is_empty_value(std::string{"x"}));

  CHECK(is_empty_value(std::string_view{}));
  CHECK_FALSE(is_empty_value(std::string_view{"x"}));

  CHECK(is_empty_value(std::map<std::string, int>{}));
  CHECK_FALSE(is_empty_value(std::map<std::string, int>{{"k", 1}}));

  CHECK(is_empty_value(std::array<char, 4>{}));
  CHECK_FALSE(is_empty_value(std::array<char, 4>{'a', 'b'}));

  const char* null_text = nullptr;
  const char* no_text   = "";
  const char* some_text = "x";
  CHECK(is_empty_value(null_text));
  CHECK(is_empty_value(no_text));
  CHECK_FALSE(is_empty_value(some_text));
}
