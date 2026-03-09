#include <doctest/doctest.h>

import reflex.core;
import std;

using namespace reflex;

TEST_CASE("reflex::to_tuple")
{
  struct s1
  {
    int a;
    int b;
  };
  auto t = to_tuple(s1{42, 43});
  CHECK(std::tuple_size_v<decltype(t)> == 2);
  CHECK(std::get<0>(t) == 42);
  CHECK(std::get<1>(t) == 43);
}