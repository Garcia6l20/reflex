// A whole namespace, bound in one line. Its types, its free functions and its
// read-only values reach Python; a nested namespace only does when it asks to.
#include <reflex/py.hpp>

#include <nanobind/stl/string.h>

#include <string>

namespace py = reflex::py;

namespace mylib
{
  struct point
  {
    int x = 0;
    int y = 0;

    point() = default;
    point(int a, int b) : x{a}, y{b}
    {}

    auto sum() const -> int
    {
      return x + y;
    }
  };

  enum class axis
  {
    horizontal,
    vertical,
  };

  struct [[= py::skip]] internal_state
  {
    int hidden = 0;
  };

  auto make(int a, int b) -> point
  {
    return point{a, b};
  }

  // Not reached. A free operator belongs on the Python type of its left
  // operand, and the namespace walk does not attach one there.
  auto operator+(point const& a, point const& b) -> point
  {
    return point{a.x + b.x, a.y + b.y};
  }

  auto describe(point const& p) -> std::string
  {
    return std::to_string(p.x) + "," + std::to_string(p.y);
  }

  auto scale(point const& p, int by) -> point
  {
    return point{p.x * by, p.y * by};
  }
  auto scale(point const& p, double by) -> point
  {
    return point{static_cast<int>(p.x * by), static_cast<int>(p.y * by)};
  }
  auto scale(point const& p) -> point
  {
    return scale(p, 2);
  }

  [[= py::skip]] auto internal_only(int n) -> int
  {
    return n;
  }

  // A parameter named self is fine here: a free function has no object for it
  // to collide with.
  auto reflect(int self) -> int
  {
    return -self;
  }

  inline constexpr int version = 3;
  inline const int    revision = 7;
  inline int          mutable_counter = 0;

  namespace detail
  {
    auto helper() -> int
    {
      return 1;
    }
  } // namespace detail

  namespace [[= py::submodule]] extras
  {
    auto extra() -> int
    {
      return 2;
    }

    inline constexpr int level = 9;
  } // namespace extras
} // namespace mylib

REFLEX_PY_MODULE(library, m)
{
  m.bind<^^mylib>();
}
