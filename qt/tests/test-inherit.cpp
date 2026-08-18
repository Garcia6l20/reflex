#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <string_view>

struct base_object : reflex::qt::object<base_object>
{
  signal<int> baseSig{this};

  [[= slot]] void baseSlot(int a)
  {
    ++base_slot_calls;
    last_base_arg = a;
  }

  [[= invocable]] int baseCalc(int a) const
  {
    return a + level;
  }

  [[= prop{}]] int level = 0;

  int base_slot_calls = 0;
  int last_base_arg   = -1;
};

struct derived_object : reflex::qt::object<derived_object, base_object>
{
  signal<QString> derivedSig{this};

  [[= slot]] void derivedSlot(QString const& s)
  {
    ++derived_slot_calls;
    name = s;
  }

  [[= invocable]] QString tag() const
  {
    return name;
  }

  [[= prop{}]] int depth = 0;

  QString name;
  int     derived_slot_calls = 0;
};

struct int_sink : reflex::qt::object<int_sink>
{
  [[= slot]] void onInt(int a)
  {
    ++calls;
    last = a;
  }

  int calls = 0;
  int last  = -1;
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

TEST_CASE("a reflex superclass produces the offsets moc would produce")
{
  const QMetaObject& parent = base_object::staticMetaObject;
  const QMetaObject& child  = derived_object::staticMetaObject;

  CHECK(std::string_view{parent.className()} == "base_object");
  CHECK(std::string_view{child.className()} == "derived_object");

  CHECK(parent.superClass() == &QObject::staticMetaObject);
  CHECK(child.superClass() == &parent);

  CHECK(parent.methodOffset() == QObject::staticMetaObject.methodCount());
  CHECK(parent.methodCount() == parent.methodOffset() + 4);
  CHECK(child.methodOffset() == parent.methodCount());
  CHECK(child.methodCount() == child.methodOffset() + 4);

  CHECK(parent.propertyOffset() == QObject::staticMetaObject.propertyCount());
  CHECK(parent.propertyCount() == parent.propertyOffset() + 1);
  CHECK(child.propertyOffset() == parent.propertyCount());
  CHECK(child.propertyCount() == child.propertyOffset() + 1);

  const QList<QByteArray> parent_expected{
      "baseSig(int)", "levelChanged()", "baseSlot(int)", "baseCalc(int)"};
  CHECK(local_signatures(parent) == parent_expected);

  const QList<QByteArray> child_expected{
      "derivedSig(QString)", "depthChanged()", "derivedSlot(QString)", "tag()"};
  CHECK(local_signatures(child) == child_expected);
}

TEST_CASE("both levels keep moc's method kinds and property notify indices")
{
  const QMetaObject& parent = base_object::staticMetaObject;
  const QMetaObject& child  = derived_object::staticMetaObject;

  CHECK(parent.method(parent.methodOffset() + 0).methodType() == QMetaMethod::Signal);
  CHECK(parent.method(parent.methodOffset() + 1).methodType() == QMetaMethod::Signal);
  CHECK(parent.method(parent.methodOffset() + 2).methodType() == QMetaMethod::Slot);
  CHECK(parent.method(parent.methodOffset() + 3).methodType() == QMetaMethod::Method);
  CHECK(parent.method(parent.methodOffset() + 3).isConst());

  CHECK(child.method(child.methodOffset() + 0).methodType() == QMetaMethod::Signal);
  CHECK(child.method(child.methodOffset() + 1).methodType() == QMetaMethod::Signal);
  CHECK(child.method(child.methodOffset() + 2).methodType() == QMetaMethod::Slot);
  CHECK(child.method(child.methodOffset() + 3).methodType() == QMetaMethod::Method);
  CHECK(child.method(child.methodOffset() + 3).isConst());

  const auto level = parent.property(parent.propertyOffset());
  CHECK(std::string_view{level.name()} == "level");
  CHECK(level.notifySignalIndex() == parent.methodOffset() + 1);
  CHECK(level.notifySignal().methodSignature() == QByteArrayLiteral("levelChanged()"));

  const auto depth = child.property(child.propertyOffset());
  CHECK(std::string_view{depth.name()} == "depth");
  CHECK(depth.notifySignalIndex() == child.methodOffset() + 1);
  CHECK(depth.notifySignal().methodSignature() == QByteArrayLiteral("depthChanged()"));
}

TEST_CASE("a derived instance reports the derived metaobject")
{
  derived_object d;
  base_object    b;

  CHECK(d.metaObject() == &derived_object::staticMetaObject);
  CHECK(b.metaObject() == &base_object::staticMetaObject);
  CHECK(d.inherits("base_object"));
  CHECK(d.inherits("QObject"));
}

TEST_CASE("a signal declared in the base fires once from a derived instance")
{
  derived_object d;
  int            seen = 0;
  int            got  = -1;

  QObject::connect(&d, &derived_object::baseSig, [&seen, &got](int a) {
    ++seen;
    got = a;
  });

  d.baseSig(7);
  CHECK(seen == 1);
  CHECK(got == 7);

  int_sink sink;
  REQUIRE(QObject::connect(&d, SIGNAL(baseSig(int)), &sink, SLOT(onInt(int))));

  d.baseSig(9);
  CHECK(seen == 2);
  CHECK(got == 9);
  CHECK(sink.calls == 1);
  CHECK(sink.last == 9);
}

TEST_CASE("a signal declared in the derived fires from a derived instance")
{
  derived_object sender;
  derived_object receiver;

  QObject::connect(&sender, &derived_object::derivedSig, &receiver, &derived_object::derivedSlot);

  sender.derivedSig(QStringLiteral("hello"));
  CHECK(receiver.derived_slot_calls == 1);
  CHECK(receiver.name == QStringLiteral("hello"));
}

TEST_CASE("a base-declared slot and invocable answer by name on a derived instance")
{
  derived_object d;

  REQUIRE(QMetaObject::invokeMethod(&d, "baseSlot", Q_ARG(int, 11)));
  CHECK(d.base_slot_calls == 1);
  CHECK(d.last_base_arg == 11);

  REQUIRE(QMetaObject::invokeMethod(&d, "derivedSlot", Q_ARG(QString, QStringLiteral("x"))));
  CHECK(d.derived_slot_calls == 1);
  CHECK(d.name == QStringLiteral("x"));

  d.level      = 5;
  int computed = 0;
  REQUIRE(QMetaObject::invokeMethod(&d, "baseCalc", Q_RETURN_ARG(int, computed), Q_ARG(int, 1)));
  CHECK(computed == 6);

  QString tag;
  REQUIRE(QMetaObject::invokeMethod(&d, "tag", Q_RETURN_ARG(QString, tag)));
  CHECK(tag == QStringLiteral("x"));
}

TEST_CASE("a base property reads and writes through QObject::property on a derived instance")
{
  derived_object d;
  int            level_notified = 0;
  int            depth_notified = 0;

  QObject::connect(
      &d, &base_object::propertyChanged<"level">, [&level_notified] { ++level_notified; });
  QObject::connect(
      &d, &derived_object::propertyChanged<"depth">, [&depth_notified] { ++depth_notified; });

  REQUIRE(d.setProperty("level", 5));
  CHECK(d.level == 5);
  CHECK(d.property("level").toInt() == 5);
  CHECK(level_notified == 1);
  CHECK(depth_notified == 0);

  REQUIRE(d.setProperty("depth", 3));
  CHECK(d.depth == 3);
  CHECK(d.property("depth").toInt() == 3);
  CHECK(level_notified == 1);
  CHECK(depth_notified == 1);

  d.setProperty<"depth">(4);
  CHECK(d.property<"depth">() == 4);
  CHECK(depth_notified == 2);
}

TEST_CASE("qobject_cast reaches both levels of the hierarchy")
{
  derived_object d;
  base_object    b;
  QObject*       as_object = &d;

  CHECK(qobject_cast<derived_object*>(as_object) == &d);
  CHECK(qobject_cast<base_object*>(as_object) == static_cast<base_object*>(&d));
  CHECK(qobject_cast<QObject*>(as_object) == as_object);

  QObject* base_only = &b;
  CHECK(qobject_cast<derived_object*>(base_only) == nullptr);
  CHECK(qobject_cast<base_object*>(base_only) == &b);
}
