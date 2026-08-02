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

    // An ordinary identifier in C++, a keyword in Python. Passed through: it is
    // reachable as **{"lambda": ...} and nothing else collides with it. Only a
    // parameter named self is rejected, and that is the object's name.
    auto keyword_named(int lambda) const -> int
    {
      return lambda;
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
