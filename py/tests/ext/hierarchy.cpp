// Bases, nested enums and nested classes. Binding a derived type binds its base
// first: nanobind resolves a caster lazily, but a class_ names its base at
// registration and the base has to exist by then.
#include <reflex/py.hpp>

namespace py = reflex::py;

namespace
{
  struct shape
  {
    shape() = default;

    auto name() const -> int
    {
      return 1;
    }

    virtual auto area() const -> int
    {
      return 0;
    }

    virtual ~shape() = default;
  };

  struct square : shape
  {
    square() = default;

    auto area() const -> int override
    {
      return side * side;
    }

    int side = 2;
  };

  // The base is bound once even though two classes reach it.
  struct circle : shape
  {
    circle() = default;

    auto area() const -> int override
    {
      return 3;
    }
  };

  struct [[= py::skip]] tag
  {
    int marker = 0;
  };

  // The only base is skipped, so this binds with no base at all.
  struct untagged : tag
  {
    untagged() = default;

    auto value() const -> int
    {
      return 4;
    }
  };

  struct hidden_base
  {
    int leaked = 0;
  };

  struct derived_privately : private hidden_base
  {
    derived_privately() = default;

    auto value() const -> int
    {
      return 5;
    }
  };

  struct host
  {
    // No annotation on an enumerator: GCC 16 rejects one, it only accepts an
    // annotation on the enumeration itself. So the naming policy is the only
    // way to respell an enumerator.
    enum class [[= py::naming::pascal_case]] colour
    {
      red,
      green,
      light_blue,
    };

    enum class [[= py::doc{"a documented enumeration"}]] state
    {
      idle,
      running,
    };

    struct inner
    {
      inner() = default;

      auto value() const -> int
      {
        return 6;
      }

      int held = 7;
    };

    struct [[= py::skip]] secret
    {
      int nothing = 0;
    };

    host() = default;

    auto tint() const -> colour
    {
      return colour::green;
    }

    auto make() const -> inner
    {
      return inner{};
    }
  };

  enum class standalone
  {
    one,
    two,
  };
} // namespace

REFLEX_PY_MODULE(hierarchy, m)
{
  m.bind<square>();
  m.bind<circle>();
  m.bind<untagged>();
  m.bind<derived_privately>();
  m.bind<host>();
  m.bind_enum<standalone>();
  // Idempotent: shape was published as square's base.
  m.bind<shape>();
}
