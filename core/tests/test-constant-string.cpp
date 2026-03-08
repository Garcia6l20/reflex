#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;

template <constant_string S>
struct use_string {
  static constexpr std::string_view s = S;
};

TEST_CASE("reflex::constant_string: basics")
{
  static_assert(use_string<"hello">::s == "hello");
}
