#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <string_view>

using namespace reflex::qt;

struct custom
{
  int value = 0;

  bool operator==(custom const&) const = default;
};

struct [[= reflex::qt::classinfo{"author", "reflex"}]] counter : reflex::qt::object<counter>
{
  signal<>                       emptySig{this};
  signal<int, with_default<int>> intSig{this, 42};
  signal<custom>                 customSig{this};

  [[= slot]] void emptySlot()
  {
    ++empty_calls;
  }

  [[= slot]] void intSlot(int a = 0, int b = 1)
  {
    ++int_calls;
    last_a = a;
    last_b = b;
  }

  [[= slot]] void customSlot(custom const& c)
  {
    last_custom = c;
  }

  [[= slot]] void stringSlot(QString const& s)
  {
    last_string = s;
  }

  [[= invocable]] int add(int a, int b = 3) const
  {
    return a + b;
  }

  [[= invocable]] custom twice(custom c)
  {
    return custom{2 * c.value};
  }

  [[= prop{}]] int    value = 0;
  [[= prop{}]] custom tag;

  int     empty_calls = 0;
  int     int_calls   = 0;
  int     last_a      = -1;
  int     last_b      = -1;
  custom  last_custom;
  QString last_string;
};

struct bare : reflex::qt::object<bare>
{
};

struct listy : reflex::qt::object<listy>
{
  [[= prop{}]] QList<custom> tags;
};

namespace
{
QList<QByteArray> local_signatures(const QMetaObject& mo)
{
  QList<QByteArray> out;
  for(int i = mo.methodOffset(); i < mo.methodCount(); ++i)
  {
    out.push_back(mo.method(i).methodSignature());
  }
  return out;
}
}

TEST_CASE("the method table matches what moc would publish")
{
  const QMetaObject& mo = counter::staticMetaObject;

  CHECK(std::string_view{mo.className()} == "counter");
  CHECK(mo.superClass() == &QObject::staticMetaObject);

  const QList<QByteArray> expected{"emptySig()",
                                   "intSig(int,int)",
                                   "intSig(int)",
                                   "customSig(custom)",
                                   "valueChanged()",
                                   "tagChanged()",
                                   "emptySlot()",
                                   "intSlot(int,int)",
                                   "intSlot(int)",
                                   "intSlot()",
                                   "customSlot(custom)",
                                   "stringSlot(QString)",
                                   "add(int,int)",
                                   "add(int)",
                                   "twice(custom)"};
  CHECK(local_signatures(mo) == expected);
}

TEST_CASE("method kinds, clone flags and constness match moc")
{
  const QMetaObject& mo     = counter::staticMetaObject;
  const int          offset = mo.methodOffset();

  CHECK(mo.method(offset + 0).methodType() == QMetaMethod::Signal);
  CHECK(mo.method(offset + 4).methodType() == QMetaMethod::Signal);
  CHECK(mo.method(offset + 6).methodType() == QMetaMethod::Slot);
  CHECK(mo.method(offset + 12).methodType() == QMetaMethod::Method);

  CHECK((mo.method(offset + 1).attributes() & QMetaMethod::Cloned) == 0);
  CHECK((mo.method(offset + 2).attributes() & QMetaMethod::Cloned) != 0);
  CHECK((mo.method(offset + 9).attributes() & QMetaMethod::Cloned) != 0);
  CHECK((mo.method(offset + 13).attributes() & QMetaMethod::Cloned) != 0);

  CHECK(mo.method(offset + 12).isConst());
  CHECK(not mo.method(offset + 14).isConst());

  CHECK(mo.method(offset + 14).returnMetaType() == QMetaType::fromType<custom>());
  CHECK(mo.method(offset + 3).parameterMetaType(0) == QMetaType::fromType<custom>());
}

TEST_CASE("class infos and properties survive the object half")
{
  const QMetaObject& mo = counter::staticMetaObject;

  REQUIRE(mo.classInfoCount() == 1);
  CHECK(std::string_view{mo.classInfo(0).name()} == "author");

  REQUIRE(mo.propertyCount() - mo.propertyOffset() == 2);

  const auto value = mo.property(mo.propertyOffset() + 0);
  CHECK(std::string_view{value.name()} == "value");
  CHECK(value.isWritable());
  CHECK(value.hasNotifySignal());
  CHECK(value.notifySignal().methodSignature() == QByteArrayLiteral("valueChanged()"));

  const auto tag = mo.property(mo.propertyOffset() + 1);
  CHECK(std::string_view{tag.name()} == "tag");
  CHECK(tag.metaType() == QMetaType::fromType<custom>());
  CHECK(tag.notifySignal().methodSignature() == QByteArrayLiteral("tagChanged()"));
}

TEST_CASE("properties round-trip through qt_metacall")
{
  counter c;

  CHECK(c.setProperty("value", 42));
  CHECK(c.value == 42);
  CHECK(c.property("value").toInt() == 42);

  CHECK(c.setProperty("tag", QVariant::fromValue(custom{5})));
  CHECK(c.tag == custom{5});
  CHECK(c.property("tag").value<custom>() == custom{5});

  c.setProperty<"value">(7);
  CHECK(c.property<"value">() == 7);
}

TEST_CASE("invokeMethod reaches slots and invocables by name")
{
  counter c;

  REQUIRE(QMetaObject::invokeMethod(&c, "intSlot", Q_ARG(int, 5), Q_ARG(int, 6)));
  CHECK(c.last_a == 5);
  CHECK(c.last_b == 6);

  REQUIRE(QMetaObject::invokeMethod(&c, "intSlot", Q_ARG(int, 9)));
  CHECK(c.last_a == 9);
  CHECK(c.last_b == 1);

  REQUIRE(QMetaObject::invokeMethod(&c, "intSlot"));
  CHECK(c.last_a == 0);
  CHECK(c.last_b == 1);

  int sum = 0;
  REQUIRE(QMetaObject::invokeMethod(&c, "add", Q_RETURN_ARG(int, sum), Q_ARG(int, 2), Q_ARG(int, 3)));
  CHECK(sum == 5);

  REQUIRE(QMetaObject::invokeMethod(&c, "add", Q_RETURN_ARG(int, sum), Q_ARG(int, 2)));
  CHECK(sum == 5);

  custom doubled{};
  REQUIRE(QMetaObject::invokeMethod(&c, "twice", Q_RETURN_ARG(custom, doubled), Q_ARG(custom, custom{21})));
  CHECK(doubled == custom{42});
}

TEST_CASE("qobject_cast works in both directions")
{
  counter  c;
  QObject* as_object = &c;

  CHECK(qobject_cast<counter*>(as_object) == &c);
  CHECK(qobject_cast<QObject*>(&c) == as_object);

  QObject plain;
  CHECK(qobject_cast<counter*>(&plain) == nullptr);
  CHECK(c.inherits("QObject"));
}

TEST_CASE("an object with nothing in it is still a usable QObject")
{
  const QMetaObject& mo = bare::staticMetaObject;

  CHECK(std::string_view{mo.className()} == "bare");
  CHECK(mo.methodCount() == mo.methodOffset());
  CHECK(mo.propertyCount() == mo.propertyOffset());
  CHECK(mo.superClass() == &QObject::staticMetaObject);

  int      destroyed = 0;
  QObject  guard;
  {
    bare b;
    QObject::connect(&b, &QObject::destroyed, &guard, [&destroyed] { ++destroyed; });
  }
  CHECK(destroyed == 1);
}

TEST_CASE("a reflex object stays a QObject to the metatype system")
{
  static_assert(QtPrivate::IsPointerToTypeDerivedFromQObject<counter*>::Value);
  static_assert(not QtPrivate::IsRealGadget<counter>::value);
  static_assert(not QtPrivate::IsPointerToGadgetHelper<counter*>::IsRealGadget);

  const auto flags = QMetaType::fromType<counter*>().flags();
  CHECK(flags.testFlag(QMetaType::PointerToQObject));
  CHECK(not flags.testFlag(QMetaType::PointerToGadget));
}

TEST_CASE("a container of a custom type round-trips as a property")
{
  listy      l;
  const auto tags = QList<custom>{custom{1}, custom{2}};

  CHECK(l.setProperty("tags", QVariant::fromValue(tags)));
  CHECK(l.tags == tags);
  CHECK(l.property("tags").value<QList<custom>>() == tags);
}
