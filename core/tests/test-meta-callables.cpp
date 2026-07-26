// Direct coverage for the reflex::meta queries the overload set work added.
// They are general purpose, so they are pinned here rather than only through
// the resolver that happens to use them.
#include <doctest/doctest.h>

#include <reflex/core.hpp>

#include <functional>
#include <string>

namespace meta_probe
{
  int  fn(int) { return 0; }
  int  fn(double, int = 1) { return 0; }
  void solo() {}

  struct thing
  {
    int  a;
    bool b;

    thing() = default;
    thing(int v) : a(v), b(false) {}
    thing(const thing&) = delete;

    int  method(int, int = 2) const { return 0; }
    void operator+=(int) {}
    int  operator[](int) const { return a; }

    int deduced(this const thing& self, int v) { return self.a + v; }

    int (*hook)(int)                          = &fn;
    std::function<double(int, int)> handler   = [](int, int) { return 0.0; };
    int                             not_a_fn  = 0;
  };

  constexpr auto lambda   = [](int, char) { return 0; };
  constexpr auto generic  = [](auto, auto) { return 0; };
  constexpr auto variadic = [](auto&&...) { return 0; };
} // namespace meta_probe

using namespace reflex;

TEST_CASE("reflex::meta: naming")
{
  SUBCASE("an ordinary function is spelled by its identifier")
  {
    static_assert(meta::spelling_of(^^meta_probe::solo) == "solo");
  }
  SUBCASE("an operator is spelled by its symbol")
  {
    constexpr auto plus_equals = [] consteval {
      return meta::functions_named(^^meta_probe::thing, "operator+=").front();
    }();
    static_assert(meta::spelling_of(plus_equals) == "operator+=");
    static_assert(not std::meta::has_identifier(plus_equals));
  }
  SUBCASE("functions_named collects every overload of one name")
  {
    static_assert(meta::functions_named(^^meta_probe, "fn").size() == 2);
    static_assert(meta::functions_named(^^meta_probe, "solo").size() == 1);
    static_assert(meta::functions_named(^^meta_probe, "absent").empty());
  }
  SUBCASE("a constructor has no spelling")
  {
    constexpr auto first = [] consteval {
      return meta::constructors_of(^^meta_probe::thing).front();
    }();
    static_assert(meta::spelling_of(first).empty());
  }
}

TEST_CASE("reflex::meta: parameters and arity")
{
  SUBCASE("parameter_types_of drops the return type")
  {
    constexpr auto method = ^^meta_probe::thing::method;
    static_assert(meta::parameter_types_of(method).size() == 2);
    static_assert(meta::parameter_types_of(method).front() == ^^int);
  }
  SUBCASE("a defaulted parameter lowers the minimum arity")
  {
    static_assert(meta::min_arity_of(^^meta_probe::thing::method) == 1);
    static_assert(meta::arities_of(^^meta_probe::thing::method).size() == 2);
    static_assert(meta::min_arity_of(^^meta_probe::solo) == 0);
    static_assert(meta::arities_of(^^meta_probe::solo).size() == 1);
  }
  SUBCASE("a function type is taken apart by decomposition")
  {
    constexpr auto params = [] consteval {
      return meta::function_type_parameters(^^int(double, char));
    };
    static_assert(params().size() == 2);
    static_assert(params()[0] == ^^double);
    static_assert(params()[1] == ^^char);
    static_assert(meta::function_type_parameters(^^void()).empty());

    // the return type needs no decomposition, return_type_of reads it directly
    static_assert(std::meta::return_type_of(^^int(double, char)) == ^^int);

    static_assert(meta::parameter_types_of(^^int(double, char)).size() == 2);
    static_assert(meta::arities_of(^^int(double, char)).size() == 1);
  }
}

TEST_CASE("reflex::meta: callables")
{
  SUBCASE("a function pointer member resolves to the function type it points at")
  {
    constexpr auto hook = ^^meta_probe::thing::hook;
    static_assert(meta::is_invocable_data_member(hook));
    static_assert(meta::callable_function_of(hook) == ^^int(int));
  }
  SUBCASE("a class with a call operator resolves through it")
  {
    constexpr auto handler = ^^meta_probe::thing::handler;
    static_assert(meta::is_invocable_data_member(handler));
    static_assert(meta::parameter_types_of(meta::callable_function_of(handler)).size() == 2);
  }
  SUBCASE("a lambda resolves through its call operator")
  {
    constexpr auto call = meta::callable_function_of(^^meta_probe::lambda);
    static_assert(std::meta::is_function(call));
    static_assert(meta::parameter_types_of(call).size() == 2);
    static_assert(meta::parameter_types_of(call).front() == ^^int);
  }
  SUBCASE("a generic lambda yields null rather than failing to compile")
  {
    // Its call operator is a template, so it has no parameter types until it is
    // substituted. Reflecting it is fine, it just does not name a function.
    static_assert(meta::callable_function_of(^^meta_probe::generic) == meta::null);
    static_assert(meta::callable_function_of(^^meta_probe::variadic) == meta::null);
  }
  SUBCASE("a data member that is not callable is not one")
  {
    static_assert(not meta::is_invocable_data_member(^^meta_probe::thing::not_a_fn));
    static_assert(meta::callable_function_of(^^meta_probe::thing::not_a_fn) == meta::null);
    static_assert(meta::callable_function_of(^^int) == meta::null);
  }
  SUBCASE("a member function is not a data member")
  {
    static_assert(not meta::is_invocable_data_member(^^meta_probe::thing::method));
  }
}

TEST_CASE("reflex::meta: explicit object parameter")
{
  SUBCASE("a deducing this member is recognised")
  {
    static_assert(meta::is_explicit_object_member_function(^^meta_probe::thing::deduced));
    static_assert(not meta::is_explicit_object_member_function(^^meta_probe::thing::method));
    static_assert(not meta::is_explicit_object_member_function(^^meta_probe::solo));
  }
  SUBCASE("the object parameter is reported and left out of the arguments")
  {
    constexpr auto deduced = ^^meta_probe::thing::deduced;
    static_assert(meta::explicit_object_type_of(deduced) == ^^const meta_probe::thing&);
    static_assert(meta::parameter_types_of(deduced).size() == 1);
    static_assert(meta::parameter_types_of(deduced).front() == ^^int);
    static_assert(meta::min_arity_of(deduced) == 1);
    static_assert(meta::explicit_object_type_of(^^meta_probe::thing::method) == meta::null);
  }
}

TEST_CASE("reflex::meta: members of a class")
{
  SUBCASE("constructors_of lists them, deleted ones included")
  {
    static_assert(meta::constructors_of(^^meta_probe::thing).size() == 3);
  }
  SUBCASE("nonstatic_data_member_types_of follows declaration order")
  {
    constexpr auto types = [] consteval {
      return meta::nonstatic_data_member_types_of(^^meta_probe::thing);
    };
    static_assert(types().size() == 5);
    static_assert(types()[0] == ^^int);
    static_assert(types()[1] == ^^bool);
  }
}
