#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;
using namespace reflex::literals;
using namespace std::literals;

TEST_CASE("reflex::core::named_tuple: basics")
{
  auto t           = make_named_tuple("a"_na = 1, "b"_na = 2.0, "c"_na = "three"sv);
  using tuple_type = std::remove_cvref_t<decltype(t)>;
  static_assert(named_tuple_c<tuple_type>);

  std::println("{}", display_string_of(type_of(^^t)));

  CHECK(t.a == 1);
  CHECK(t.b == 2.0);
  CHECK(t.c == "three");

  CHECK(get<"a">(t) == 1);
  CHECK(get<"b">(t) == 2.0);
  CHECK(get<"c">(t) == "three");

  auto& [a, b, c] = t;
  CHECK(a == 1);
  CHECK(b == 2.0);
  CHECK(c == "three");

  CHECK(get<0>(t) == 1);
  CHECK(get<1>(t) == 2.0);
  CHECK(get<2>(t) == "three");
}
