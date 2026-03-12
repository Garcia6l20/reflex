#include <doctest/doctest.h>

import reflex.core;
import test.tu_counter;
import std;

using namespace reflex;
using namespace test;

static_assert(counter::value() == 2);

consteval
{
  counter::increment();
}
static_assert(counter::value() == 3);

consteval
{
  counter::increment();
}
static_assert(counter::value() == 4);

consteval
{
  const_assert(types::size() == 1); // first type is pushed from tu-mod.cppm
  const_assert(types::all().front() == ^^test_s1, "first item should be test_s1 reflection");
  types::push(^^int);
  const_assert(types::size() == 2);
  const_assert(types::contains(^^int), "second item should be int reflection");
  types::push(^^float);
  const_assert(types::size() == 3);
  const_assert(types::contains(^^float));

  types::push(^^float);
  const_assert(types::size() == 3, "expected type_registry to be unique");
}

TEST_CASE("list types")
{
  template for(constexpr auto T : types::all())
  {
    std::println("- {}", display_string_of(T));
  }
}
