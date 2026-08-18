#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <string_view>

struct custom
{
  int value = 0;

  bool operator==(custom const&) const = default;
};

struct [[= reflex::qt::classinfo{"author", "reflex"}]] point : reflex::qt::gadget<point>
{
  [[= prop{}]] int     x = 0;
  [[= prop{}]] QString label;
  [[= prop{"r"}]] int  frozen = 7;
  [[= prop{}]] custom  tag;

  [[= invocable]] int    add(int a, int b) { return a + b; }
  [[= invocable]] custom twice(custom c) { return custom{2 * c.value}; }
};

struct empty_gadget : reflex::qt::gadget<empty_gadget>
{
};

namespace
{
namespace qtd = reflex::qt::detail;

static_assert(qtd::static_meta_type_id_of(^^int) == QMetaType::Int);
static_assert(qtd::static_meta_type_id_of(^^QString) == QMetaType::QString);
static_assert(qtd::static_meta_type_id_of(^^QObject*) == QMetaType::QObjectStar);
static_assert(qtd::static_meta_type_id_of(^^custom) == qtd::custom_type);
static_assert(qtd::static_meta_type_id_of(^^const QString&) == QMetaType::QString);
static_assert(qtd::normalized_type_name(^^custom) == "custom");
}

TEST_CASE("the class name and class infos land in the meta object")
{
  const QMetaObject& mo = point::staticMetaObject;

  CHECK(std::string_view{mo.className()} == "point");
  REQUIRE(mo.classInfoCount() == 1);
  CHECK(std::string_view{mo.classInfo(0).name()} == "author");
  CHECK(std::string_view{mo.classInfo(0).value()} == "reflex");
}

TEST_CASE("properties are described as moc would describe them")
{
  const QMetaObject& mo = point::staticMetaObject;

  REQUIRE(mo.propertyCount() == 4);

  const auto x = mo.property(0);
  CHECK(std::string_view{x.name()} == "x");
  CHECK(std::string_view{x.typeName()} == "int");
  CHECK(x.isReadable());
  CHECK(x.isWritable());

  const auto label = mo.property(1);
  CHECK(std::string_view{label.name()} == "label");
  CHECK(std::string_view{label.typeName()} == "QString");
  CHECK(label.isWritable());

  const auto frozen = mo.property(2);
  CHECK(std::string_view{frozen.name()} == "frozen");
  CHECK(frozen.isReadable());
  CHECK(not frozen.isWritable());

  const auto tag = mo.property(3);
  CHECK(std::string_view{tag.name()} == "tag");
  CHECK(std::string_view{tag.typeName()} == "custom");
  CHECK(tag.metaType() == QMetaType::fromType<custom>());
}

TEST_CASE("properties round-trip through the gadget metacall")
{
  point              p;
  const QMetaObject& mo = point::staticMetaObject;

  CHECK(mo.property(0).writeOnGadget(&p, 42));
  CHECK(p.x == 42);
  CHECK(mo.property(0).readOnGadget(&p).toInt() == 42);

  CHECK(mo.property(1).writeOnGadget(&p, QStringLiteral("hello")));
  CHECK(p.label == QStringLiteral("hello"));
  CHECK(mo.property(1).readOnGadget(&p).toString() == QStringLiteral("hello"));

  CHECK(mo.property(3).writeOnGadget(&p, QVariant::fromValue(custom{5})));
  CHECK(p.tag == custom{5});
  CHECK(mo.property(3).readOnGadget(&p).value<custom>() == custom{5});

  CHECK(not mo.property(2).writeOnGadget(&p, 1));
  CHECK(p.frozen == 7);
}

TEST_CASE("invocables are described and callable")
{
  const QMetaObject& mo = point::staticMetaObject;

  REQUIRE(mo.methodCount() == 2);

  const auto add = mo.method(0);
  CHECK(add.methodType() == QMetaMethod::Method);
  CHECK(add.name() == QByteArrayLiteral("add"));
  CHECK(add.parameterCount() == 2);
  CHECK(add.returnMetaType() == QMetaType::fromType<int>());
  CHECK(add.parameterMetaType(0) == QMetaType::fromType<int>());
  CHECK(add.parameterNames() == QList<QByteArray>{"a", "b"});

  const auto twice = mo.method(1);
  CHECK(twice.name() == QByteArrayLiteral("twice"));
  CHECK(twice.returnMetaType() == QMetaType::fromType<custom>());
  CHECK(twice.parameterMetaType(0) == QMetaType::fromType<custom>());

  point p;
  int   sum = 0;
  REQUIRE(add.invokeOnGadget(&p, qReturnArg(sum), 2, 3));
  CHECK(sum == 5);

  custom doubled{};
  REQUIRE(twice.invokeOnGadget(&p, qReturnArg(doubled), custom{21}));
  CHECK(doubled == custom{42});
}

TEST_CASE("a gadget with nothing in it still produces a valid meta object")
{
  const QMetaObject& mo = empty_gadget::staticMetaObject;

  CHECK(std::string_view{mo.className()} == "empty_gadget");
  CHECK(mo.propertyCount() == 0);
  CHECK(mo.methodCount() == 0);
  CHECK(mo.classInfoCount() == 0);
}

TEST_CASE("the gadget is a valid meta type named after the class")
{
  const auto type = QMetaType::fromType<point>();

  CHECK(type.isValid());
  CHECK(std::string_view{type.name()} == "point");
  CHECK(type.metaObject() == &point::staticMetaObject);
}

TEST_CASE("properties are reachable by name")
{
  point p;
  p.setProperty<"x">(11);
  CHECK(p.property<"x">() == 11);
}
