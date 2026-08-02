// The target program of the project: a class binds with no per-member
// declaration, and the constructors that cannot be bound drop out on their own.
#include <reflex/py.hpp>

namespace py = reflex::py;

namespace
{
  // Never defined. A constructor taking one cannot be bound: there is no caster
  // to instantiate for an incomplete type.
  struct some_type;

  class my_class
  {
  public:
    my_class(int a, int b) : sum_{a + b}
    {}
    explicit my_class(some_type const&);
    [[= py::skip]] my_class(char const*) : sum_{-1}
    {}

    auto sum() const -> int
    {
      return sum_;
    }

  private:
    int sum_;
  };

  struct empty
  {};

  struct only_default
  {
    only_default() = default;
    only_default(int) = delete;
  };

  class hidden_ctor
  {
  public:
    static auto make() -> hidden_ctor
    {
      return hidden_ctor{};
    }

  private:
    hidden_ctor() = default;
  };

  struct [[= py::rename{"renamed"}]] awkward_name
  {
    awkward_name() = default;
  };

  struct explicitly_named
  {
    explicitly_named() = default;
  };

  struct defaulted
  {
    explicit defaulted(int a, int b = 3) : sum{a + b}
    {}

    int sum;
  };
} // namespace

REFLEX_PY_MODULE(basic, m)
{
  m.bind<my_class>().def("sum", &my_class::sum);
  m.bind<empty>();
  m.bind<only_default>();
  m.bind<hidden_ctor>();
  m.bind<awkward_name>();
  m.bind<explicitly_named>("given");
  m.bind<defaulted>().def_ro("sum", &defaulted::sum);
}
