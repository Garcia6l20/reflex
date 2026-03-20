#include <doctest/doctest.h>

#include <reflex/core.hpp>

#include "reg-mod.hpp"
#include "reg-mod1.hpp"
#include "reg-mod2.hpp"

using namespace reflex;
using namespace test;

consteval
{
  const_assert(derived_registry::size() >= 2, "expected reflected types from imported TUs");
  const_assert(derived_registry::contains(^^test_s1));
  const_assert(derived_registry::contains(^^test_s2));
  const_assert(derived_registry::contains(^^register_type<int>));
  const_assert(derived_registry::contains(^^register_type<double>));
  const_assert(derived_registry::contains(^^register_type<std::string>));

  const_assert(derived_registry::contains(^^float));
  const_assert(derived_registry::contains(^^std::vector<int>));
}

TEST_CASE("reflex::derived_registry")
{
  std::println("Derived types:");
  template for(constexpr auto T : derived_registry::all())
  {
    std::println("- {}", display_string_of(T));
  }
}

TEST_CASE("reflex::annotated_registry")
{
  std::println("Annotated types:");
  template for(constexpr auto T : annotated_registry::all())
  {
    std::println("- {}", display_string_of(T));
  }
}
