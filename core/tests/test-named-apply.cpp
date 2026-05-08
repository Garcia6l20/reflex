#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;
using namespace reflex::literals;
using namespace std::literals;

double f1(int a, double b, std::string_view c)
{
  return a + b + c.size();
}

TEST_CASE("reflex::core::named_apply: basics")
{
  auto tup = named_tuple_from_function_parameters_t<^^f1>{.a = 42, .b = 3.14, .c = "hello"sv};
  std::println("{}", display_string_of(dealias(^^tup)));
  CHECK(apply<^^f1>(tup) == 42 + 3.14 + 5);
}

double f2(int a, double b = 1.0, std::string_view c = "default"sv)
{
  return a + b + c.size();
}

TEST_CASE("reflex::core::named_apply: defaulted parameters")
{
  auto tup = named_tuple_from_function_parameters_t<^^f2>{.a = 42, .b = std::nullopt, .c = std::nullopt};
  auto dropped = drop_optionals(tup);
  std::println("{}", debug(tup));
  std::println("{}", dropped);


  constexpr auto os = overload_set_of(^^f2);
  static_assert(os.is_invocable({^^int}));
  static_assert(os.is_invocable({^^int, ^^double}));

  // auto&& [a, b] = drop_back<1>(42, 1.0, "default"sv);
  // apply_impl<^^f2>(a, b);
  // std::println("{}, {}", a, b);
  // (std::println("{}", values), ...);
  // apply_drop_impl<1>(f2, 42, 1.0, "default"sv);
  // std::println("{}", display_string_of(dealias(^^tup)));
  CHECK(apply<^^f2>(tup) == 42 + 1.0 + 7);
}
