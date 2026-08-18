#include <reflex/const_check.hpp>
#include <reflex/qt.hpp>
#include <reflex/qt/moc/module.hpp>

#include <doctest/doctest.h>

namespace moc = reflex::qt::moc;
namespace qt  = reflex::qt;

namespace shapes
{
struct dot : qt::gadget<dot>
{
  [[= qt::prop{}]] int x = 0;
};

struct line : qt::object<line>
{
  [[= qt::prop{}]] int length = 0;
};

struct helper
{
  int unrelated = 0;
};

using alias = dot;

struct box;

namespace inner
{
struct deep : qt::gadget<deep>
{
  [[= qt::prop{}]] int z = 0;
};
} // namespace inner
} // namespace shapes

struct hidden_property : qt::object<hidden_property>
{
private:
  [[= qt::prop{}]] int secret = 0;
};

REFLEX_QT_MODULE(shape_types, m)
{
  m.expose<^^shapes>();
}

REFLEX_QT_MODULE(deep_types, m)
{
  m.expose<^^shapes::inner>();
  m.expose<shapes::line>();
}

REFLEX_QT_MODULE(nothing_types, m)
{
  (void)m;
}

TEST_CASE("a module body over a namespace exposes exactly its own reflex.qt classes")
{
  static constexpr auto exposed = moc::exposed_types<shape_types>;

  static_assert(exposed.size() == 2);
  static_assert(exposed[0] == ^^shapes::dot);
  static_assert(exposed[1] == ^^shapes::line);
}

TEST_CASE("a nested namespace is reached by naming it, and the order is the body's")
{
  static constexpr auto exposed = moc::exposed_types<deep_types>;

  static_assert(exposed.size() == 2);
  static_assert(exposed[0] == ^^shapes::inner::deep);
  static_assert(exposed[1] == ^^shapes::line);
}

TEST_CASE("an empty module body exposes nothing")
{
  static_assert(moc::exposed_types<nothing_types>.size() == 0);
}

TEST_CASE("exposing anything but a reachable reflex.qt class is rejected at compile time")
{
  consteval
  {
    moc::module_ m;
    REFLEX_CONSTEVAL_THROWS(m.expose<int>());
    REFLEX_CONSTEVAL_THROWS(m.expose<QObject>());
    REFLEX_CONSTEVAL_THROWS(m.expose<shapes::helper>());
    REFLEX_CONSTEVAL_THROWS(m.expose<hidden_property>());
    REFLEX_CONSTEVAL_NOTHROW(m.expose<shapes::dot>());
  }
}

TEST_CASE("a module body drops a class it names twice")
{
  consteval
  {
    moc::module_ m;
    m.expose<shapes::dot>();
    m.expose<^^shapes>();
    REFLEX_META_CHECK(m.types.size() == 2, "expose should have deduplicated", ^^shapes);
  }
}
