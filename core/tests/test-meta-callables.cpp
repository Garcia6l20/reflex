// Direct coverage for reflex::meta::callable_function_of. It is general
// purpose, so it is pinned here rather than only through the cli code that
// happens to use it.
#include <doctest/doctest.h>

#include <reflex/core.hpp>

#include <functional>

namespace meta_probe
{
  int fn(int) { return 0; }

  struct thing
  {
    int a;

    int method(int, int = 2) const { return 0; }

    int (*hook)(int)                        = &fn;
    std::function<double(int, int)> handler = [](int, int) { return 0.0; };
    int                             plain   = 0;
  };

  constexpr auto lambda   = [](int, char) { return 0; };
  constexpr auto generic  = [](auto, auto) { return 0; };
  constexpr auto variadic = [](auto&&...) { return 0; };
} // namespace meta_probe

using namespace reflex;

TEST_CASE("reflex::meta: callables")
{
  SUBCASE("a function resolves to its own type")
  {
    static_assert(meta::callable_function_of(^^meta_probe::fn) == ^^int(int));
  }
  SUBCASE("a function pointer member resolves to the function type it points at")
  {
    static_assert(meta::callable_function_of(^^meta_probe::thing::hook) == ^^int(int));
  }
  SUBCASE("a class with a call operator resolves through it")
  {
    constexpr auto handler = meta::callable_function_of(^^meta_probe::thing::handler);
    static_assert(std::meta::is_function(handler));
    static_assert(std::meta::parameters_of(handler).size() == 2);
  }
  SUBCASE("a lambda resolves through its call operator")
  {
    constexpr auto call = meta::callable_function_of(^^meta_probe::lambda);
    static_assert(std::meta::is_function(call));
    static_assert(std::meta::parameters_of(call).size() == 2);
    static_assert(std::meta::type_of(std::meta::parameters_of(call).front()) == ^^int);
  }
  SUBCASE("a generic lambda yields null rather than failing to compile")
  {
    // Its call operator is a template, so it has no parameter types until it is
    // substituted. Reflecting it is fine, it just does not name a function.
    static_assert(meta::callable_function_of(^^meta_probe::generic) == meta::null);
    static_assert(meta::callable_function_of(^^meta_probe::variadic) == meta::null);
  }
  SUBCASE("something that is not callable yields null")
  {
    static_assert(meta::callable_function_of(^^meta_probe::thing::plain) == meta::null);
    static_assert(meta::callable_function_of(^^int) == meta::null);
  }
}
