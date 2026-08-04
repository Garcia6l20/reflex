// Operators reach Python under the dunder the table gives them. The name is not
// derivable by rule: `operator+` is __add__ written binary and __pos__ written
// unary, `operator<=>` is six names at once, and several have no counterpart.
#include <reflex/py.hpp>

// A std::string crosses the boundary only with its caster included. Reflection
// cannot add an include, so this is the user's to write.
#include <nanobind/stl/string.h>

#include <compare>
#include <string>

namespace py = reflex::py;

namespace
{
  struct number
  {
    int value = 0;

    number() = default;
    explicit number(int v) : value{v}
    {}

    auto operator+(number const& other) const -> number
    {
      return number{value + other.value};
    }
    auto operator-(number const& other) const -> number
    {
      return number{value - other.value};
    }
    auto operator-() const -> number
    {
      return number{-value};
    }
    auto operator*(number const& other) const -> number
    {
      return number{value * other.value};
    }
    auto operator/(number const& other) const -> number
    {
      return number{value / other.value};
    }
    auto operator%(number const& other) const -> number
    {
      return number{value % other.value};
    }
    auto operator~() const -> number
    {
      return number{~value};
    }
    auto operator&(number const& other) const -> number
    {
      return number{value & other.value};
    }
    auto operator<<(int by) const -> number
    {
      return number{value << by};
    }

    auto operator+=(number const& other) -> number&
    {
      value += other.value;
      return *this;
    }

    auto operator()(int by) const -> int
    {
      return value * by;
    }

    // No Python counterpart. Bound under a dunder only because it is renamed.
    [[= py::rename{"__abs__"}]] auto operator!() const -> number
    {
      return number{value < 0 ? -value : value};
    }

    // Left unbound by the table and not renamed.
    auto operator++() -> number&
    {
      ++value;
      return *this;
    }

    explicit operator bool() const
    {
      return value != 0;
    }

    // Not explicit, so not bound: a silent conversion to a number is a wider
    // promise than the class made.
    operator int() const
    {
      return value;
    }
  };

  // A spaceship on its own supplies all six comparisons.
  struct ordered
  {
    int rank = 0;

    ordered() = default;
    explicit ordered(int r) : rank{r}
    {}

    auto operator<=>(ordered const& other) const = default;
  };

  // An explicit equality is more specific and wins the equality pair.
  struct specific
  {
    int rank  = 0;
    int shade = 0;

    specific() = default;
    specific(int r, int s) : rank{r}, shade{s}
    {}

    auto operator<=>(specific const& other) const
    {
      return rank <=> other.rank;
    }
    auto operator==(specific const& other) const -> bool
    {
      return rank == other.rank and shade == other.shade;
    }
  };

  struct table
  {
    int slots[4] = {0, 0, 0, 0};

    table() = default;

    auto operator[](int index) -> int&
    {
      return slots[index];
    }
  };

  // The idiomatic pair. The const preference in bindable_overloads has to keep
  // the non-const half here, or __setitem__ is never generated.
  struct paired_table
  {
    int slots[4] = {0, 0, 0, 0};

    paired_table() = default;

    auto operator[](int index) -> int&
    {
      return slots[index];
    }
    auto operator[](int index) const -> int const&
    {
      return slots[index];
    }
  };

  struct read_only_table
  {
    int slots[4] = {1, 2, 3, 4};

    read_only_table() = default;

    auto operator[](int index) const -> int
    {
      return slots[index];
    }
  };

  struct named
  {
    std::string text;

    named() = default;

    explicit operator std::string() const
    {
      return text;
    }
  };
} // namespace

REFLEX_PY_MODULE(operators, m)
{
  m.bind<number>();
  m.bind<ordered>();
  m.bind<specific>();
  m.bind<table>();
  m.bind<paired_table>();
  m.bind<read_only_table>();
  m.bind<named>();
}
