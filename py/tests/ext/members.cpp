// Data members and methods, without an overload set anywhere: the binder walks
// members_of once and emits one attribute per member that survives the filter.
#include <reflex/py.hpp>

namespace py = reflex::py;

namespace
{
  struct some_type;

  struct widget
  {
    int                    value  = 0;
    int const              fixed  = 7;
    [[= py::readonly]] int serial = 42;
    [[= py::skip]] int     internal = 0;

    static int made;

    // Not published: there is no caster for an array and def_rw's setter
    // cannot assign to one.
    int slots[3] = {0, 0, 0};

    auto doubled() const -> int
    {
      return value * 2;
    }

    auto bump(int by) -> int
    {
      value += by;
      return value;
    }

    static auto tag() -> int
    {
      return 99;
    }

    [[= py::skip]] auto hidden() const -> int
    {
      return 1;
    }

    [[= py::rename{"renamed"}]] auto original() const -> int
    {
      return 2;
    }

    // Never bound: no parameter types until it is substituted.
    template <typename U> auto generic(U u) const -> U
    {
      return u;
    }

    // Never bound: an incomplete type has no caster.
    auto opaque(some_type const&) const -> int;
    auto returns_opaque() const -> some_type const&;

    // Step 08's job. Binding it here would make an unreachable "operator+".
    auto operator+(widget const& other) const -> widget
    {
      return widget{value + other.value};
    }

    explicit operator bool() const
    {
      return value != 0;
    }
  };

  int widget::made = 5;

  class guarded
  {
  public:
    guarded() = default;

    auto visible() const -> int
    {
      return secret_;
    }

  private:
    int  secret_ = 3;
    auto invisible() const -> int
    {
      return 4;
    }
  };

  struct bare
  {};
} // namespace

REFLEX_PY_MODULE(members, m)
{
  m.bind<widget>();
  m.bind<guarded>();
  m.bind<bare>();
}
