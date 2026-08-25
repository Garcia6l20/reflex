#include <doctest/doctest.h>

#include <reflex/qt.hpp>
#include <reflex/qt/debug.hpp>

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QMetaType>
#include <QtCore/QVariant>

#include <string>
#include <type_traits>

namespace qt  = reflex::qt;
namespace qtd = reflex::qt::detail;

namespace adopt_test
{
struct point
{
  [[= qt::prop{}]] int x = 0;
  [[= qt::prop{}]] int y = 0;

  [[= qt::invocable]] int norm2() const
  {
    return x * x + y * y;
  }

  [[= qt::slot]] void scale(int by)
  {
    x *= by;
    y *= by;
  }
};

struct twin : qt::gadget<twin>
{
  [[= qt::prop{}]] int x = 0;
  [[= qt::prop{}]] int y = 0;

  [[= qt::invocable]] int norm2() const
  {
    return x * x + y * y;
  }

  [[= qt::slot]] void scale(int by)
  {
    x *= by;
    y *= by;
  }
};

struct plain
{
  int x = 0;
};

struct signalling
{
  qtd::signal_decl<signalling, int> beeped{nullptr};
};
}

consteval
{
  qt::make_gadget(^^adopt_test::point);
}

using namespace adopt_test;

TEST_CASE("an adopted struct and a CRTP twin publish the same tables")
{
  const QMetaObject& adopted = qt::meta_object_of<point>();
  const QMetaObject& crtp    = twin::staticMetaObject;

  REQUIRE(adopted.propertyCount() == crtp.propertyCount());
  for(int i = 0; i < adopted.propertyCount(); ++i)
  {
    CHECK(std::string{adopted.property(i).name()} == std::string{crtp.property(i).name()});
    CHECK(std::string{adopted.property(i).typeName()} == std::string{crtp.property(i).typeName()});
  }

  REQUIRE(adopted.methodCount() == crtp.methodCount());
  for(int i = 0; i < adopted.methodCount(); ++i)
  {
    CHECK(adopted.method(i).methodSignature() == crtp.method(i).methodSignature());
    CHECK(adopted.method(i).methodType() == crtp.method(i).methodType());
  }

  CHECK(std::string{adopted.className()} == "adopt_test::point");
  CHECK(std::string{crtp.className()} == "adopt_test::twin");
}

TEST_CASE("an adopted struct stays an aggregate the caller braces flat")
{
  const point p{3, 4};
  const twin  t{{}, 3, 4};

  CHECK(p.norm2() == t.norm2());
  CHECK(std::is_aggregate_v<point>);
  CHECK(sizeof(point) == 2 * sizeof(int));
}

TEST_CASE("a property of an adopted struct is read and written through its metaobject")
{
  const QMetaObject& mo = qt::meta_object_of<point>();
  point              p{3, 4};

  const int index = mo.indexOfProperty("x");
  REQUIRE(index >= 0);
  CHECK(mo.property(index).readOnGadget(&p).toInt() == 3);
  REQUIRE(mo.property(index).writeOnGadget(&p, 7));
  CHECK(p.x == 7);
}

TEST_CASE("a method of an adopted struct is invoked through its metaobject")
{
  const QMetaObject& mo = qt::meta_object_of<point>();
  point              p{3, 4};

  int result = 0;
  REQUIRE(mo.method(mo.indexOfMethod("norm2()")).invokeOnGadget(&p, qReturnArg(result)));
  CHECK(result == 25);

  REQUIRE(mo.method(mo.indexOfMethod("scale(int)")).invokeOnGadget(&p, Q_ARG(int, 2)));
  CHECK(p.x == 6);
  CHECK(p.y == 8);
}

TEST_CASE("Qt sees an adopted struct as a gadget carrying its metaobject")
{
  const QMetaType type = QMetaType::fromType<point>();

  CHECK(type.flags().testFlag(QMetaType::IsGadget));
  CHECK(type.metaObject() == &qt::meta_object_of<point>());
  CHECK(std::string{type.name()} == "adopt_test::point");

  CHECK(QMetaType::fromType<twin>().flags().testFlag(QMetaType::IsGadget));
  CHECK(not QMetaType::fromType<plain>().flags().testFlag(QMetaType::IsGadget));
}

TEST_CASE("an adopted struct crosses a QVariant unchanged")
{
  const QVariant v = QVariant::fromValue(point{3, 4});

  REQUIRE(v.metaType() == QMetaType::fromType<point>());
  CHECK(std::string{v.metaType().name()} == "adopt_test::point");
  CHECK(v.value<point>().norm2() == 25);
}

TEST_CASE("the mark answers both the type query and the reflection query")
{
  CHECK(qt::adopted<point>);
  CHECK(not qt::adopted<twin>);
  CHECK(not qt::adopted<plain>);

  consteval
  {
    REFLEX_CONSTEVAL_NOTHROW(qt::is_adopted(^^point));
  }

  static_assert(qt::is_adopted(^^point));
  static_assert(not qt::is_adopted(^^twin));
  static_assert(not qt::is_adopted(^^plain));
  static_assert(not qt::is_adopted(^^int));
}

TEST_CASE("adopting something that is already a reflex.qt class is rejected")
{
  consteval
  {
    REFLEX_CONSTEVAL_THROWS_WITH("already derives reflex::qt::gadget", qt::make_gadget(^^twin));
    REFLEX_CONSTEVAL_THROWS_WITH("is not a complete class type", qt::make_gadget(^^int));
  }
}

TEST_CASE("a signal on an adopted struct hits the gadget diagnostic")
{
  consteval
  {
    REFLEX_CONSTEVAL_THROWS_WITH("has no QMetaObject::activate to emit it",
                                 qtd::validate_gadget_members(^^adopt_test::signalling));
  }
}

TEST_CASE("describe reads an adopted struct the way it reads a gadget")
{
  const std::string text = qt::describe(qt::meta_object_of<point>());

  CHECK(text.contains("class adopt_test::point"));
  CHECK(text.contains("property    x : int"));
  CHECK(text.contains("method      norm2()"));
  CHECK(text.contains("slot        scale(int)"));
}
