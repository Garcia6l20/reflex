export module serde.tests.types;

import reflex.serde;
import std;

using namespace reflex;

// Fixtures shared by the backend test suites.
export {
  enum class[[= derive(Format, Parse)]] Color
  {
    Red,
    Green,
    Blue
  };

  struct[[= derive(Debug)]] Opt
  {
    std::string        name;
    std::optional<int> count;

    constexpr bool operator==(Opt const& other) const = default;
  };

  struct[[= derive(Debug)]] Enumed
  {
    std::string name;
    Color       color;

    constexpr bool operator==(Enumed const& other) const = default;
  };
}
