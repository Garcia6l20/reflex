export module test.tu_counter;

import reflex.core;
import std;

export namespace test
{
template <auto> struct test_scope;

using counter = reflex::meta::tu::counter<test_scope>;

static_assert(counter::value() == 0);

consteval
{
  counter::increment();
}
static_assert(counter::value() == 1);

consteval
{
  counter::increment();
}
static_assert(counter::value() == 2);

template <std::size_t> struct types_registry_scope;
using types = reflex::meta::tu::type_registry<types_registry_scope>;

struct test_s1
{};
consteval
{
  types::push(^^test_s1);
}

} // namespace test
