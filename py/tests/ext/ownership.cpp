// What a bound call does with the result it returns. The type settles some of
// it, and a py::returns settles the rest: a pointer handing back something
// borrowed and one handing back a fresh allocation are the same type.
#include <reflex/py.hpp>

#include <nanobind/stl/unique_ptr.h>

#include <memory>

namespace py = reflex::py;

namespace
{
  struct config
  {
    int level = 0;
  };

  struct counted
  {
    static int alive;

    counted()
    {
      ++alive;
    }
    counted(counted const&)
    {
      ++alive;
    }
    ~counted()
    {
      --alive;
    }
  };

  int counted::alive = 0;

  struct engine
  {
    config held;
    config borrowed;

    engine() = default;

    // Inferred reference_internal: a caller mutating the result is mutating the
    // engine, and the engine outlives the view.
    auto settings() -> config&
    {
      return held;
    }

    auto readonly_settings() const -> config const&
    {
      return held;
    }

    // The type cannot say whether this is borrowed or freshly made, so it is
    // annotated.
    [[= py::returns{py::nb::rv_policy::reference}]] auto find() -> config*
    {
      return &borrowed;
    }

    auto fresh() -> std::unique_ptr<counted>
    {
      return std::make_unique<counted>();
    }

    auto by_value() const -> config
    {
      return held;
    }

    static auto alive() -> int
    {
      return counted::alive;
    }
  };
} // namespace

REFLEX_PY_MODULE(ownership, m)
{
  m.bind<config>();
  m.bind<counted>();
  m.bind<engine>();
}
