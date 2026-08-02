// Overload sets, driven off reflex::overloads_of: several members share a name,
// a defaulted parameter is reachable at more than one arity, and a signature
// naming an incomplete type is not reachable at all.
#include <reflex/py.hpp>

namespace py = reflex::py;

namespace
{
  struct some_type;

  struct widget
  {
    int value = 0;

    auto method() const -> int
    {
      return 1;
    }
    auto method(int n) const -> int
    {
      return n * 10;
    }
    auto method(some_type const&) const -> int;

    auto scaled(int n, int k = 2) const -> int
    {
      return n * k;
    }

    // Only the short arity survives: the defaulted parameter is unbindable, and
    // parameter_types() truncates to the candidate's arity.
    auto partly(int n, some_type const* p = nullptr) const -> int;

    auto once() const -> int
    {
      return 4;
    }
    auto once() -> int
    {
      return 5;
    }

    auto only_rvalue() && -> int
    {
      return 6;
    }
    auto by_lvalue() & -> int
    {
      return 7;
    }

    auto at(this widget const& self, int n) -> int
    {
      return self.value + n;
    }

    auto pick(double) const -> int
    {
      return 100;
    }
    auto pick(int) const -> int
    {
      return 200;
    }

    static auto make() -> int
    {
      return 8;
    }
    static auto make(int n) -> int
    {
      return n;
    }
    static auto counted(int n, int by = 1) -> int
    {
      return n + by;
    }

    auto both() const -> int
    {
      return 9;
    }
    [[= py::skip]] auto both(int) const -> int
    {
      return 10;
    }

    [[= py::rename{"renamed"}]] auto original() const -> int
    {
      return 11;
    }
    auto original(int n) const -> int
    {
      return n;
    }
  };

  auto widget::partly(int n, some_type const*) const -> int
  {
    return n;
  }
} // namespace

REFLEX_PY_MODULE(overloads, m)
{
  m.bind<widget>();
}
