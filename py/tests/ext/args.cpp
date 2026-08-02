// Argument names come from the parameters, docstrings from py::doc. There is no
// way to reach a /// comment through reflection, so an annotation is the only
// source of one.
#include <reflex/py.hpp>

namespace py = reflex::py;

namespace
{
  struct [[= py::doc{"a documented type"}]] widget
  {
    [[= py::doc{"how many times it turned"}]] int count = 0;

    auto scaled(int n, int k = 2) const -> int
    {
      return n * k;
    }

    // No identifier on the second parameter, so the whole call falls back to
    // positional rather than emitting a short nb::arg pack.
    auto partly_named(int named, int) const -> int
    {
      return named;
    }

    auto nothing() const -> int
    {
      return 1;
    }

    // The object is not an argument. The first Python argument is n.
    auto at(this widget const& self, int n) -> int
    {
      return self.count + n;
    }

    static auto twice(int value) -> int
    {
      return value * 2;
    }

    [[= py::doc{"turns\nand \"turns\""}]] auto documented(int by) const -> int
    {
      return by;
    }

    // Ordinary identifiers in C++, awkward in Python. Passed through, so only a
    // keyword call is affected.
    auto shadowing(int self, int lambda) const -> int
    {
      return self + lambda;
    }

    auto undocumented(int n) const -> int
    {
      return n;
    }
  };

  struct plain
  {
    int value = 0;
  };
} // namespace

REFLEX_PY_MODULE(args, m)
{
  m.bind<widget>();
  m.bind<plain>();
}
