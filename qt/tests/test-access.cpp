#include <doctest/doctest.h>

#include <reflex/const_check.hpp>
#include <reflex/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>

#include <chrono>
#include <functional>
#include <string_view>
#include <utility>

using namespace std::chrono_literals;

struct controller : reflex::qt::object<controller>
{
  friend reflex::qt::access<controller>;

  int seen     = 0;
  int ticks    = 0;
  int listened = 0;

  void fire(int n)
  {
    changed(n);
  }

  QMetaObject::Connection on_changed(std::function<void(int)> f)
  {
    return connect(this, &controller::changed, std::move(f));
  }

  int start()
  {
    return startTimer<^^tick>(1);
  }

  bool stop()
  {
    return killTimer<^^tick>();
  }

private:
  enum class mode
  {
    idle,
    busy
  };

  signal<int> changed{this};

  [[= prop{}]] int count = 0;

  [[= getter<^^count>]] int getCount() const
  {
    return count * 2;
  }

  [[= setter<^^count>]] void setCount(int value)
  {
    count = value / 2;
  }

  [[= listener<^^count>]] void onCountChanged()
  {
    ++listened;
  }

  [[= slot]] void onThing(int n)
  {
    seen += n;
  }

  [[= invocable]] int doubled() const
  {
    return count * 2;
  }

  void tick()
  {
    ++ticks;
  }

  timer<^^tick> tick_timer;
};

struct friended : reflex::qt::gadget<friended>
{
  friend reflex::qt::access<friended>;

private:
  [[= prop{}]] int hidden = 3;

  [[= invocable]] int twice() const
  {
    return 2 * hidden;
  }
};

struct unfriended : reflex::qt::gadget<unfriended>
{
private:
  [[= prop{}]] int hidden = 3;

  [[= invocable]] int twice() const
  {
    return 2 * hidden;
  }
};

namespace
{
consteval auto member(std::meta::info owner, std::string_view name) -> std::meta::info
{
  return reflex::meta::member_named(owner, name, reflex::meta::access_context::unchecked());
}

static_assert(reflex::qt::access<friended>::reachable(member(^^friended, "twice")));
static_assert(reflex::qt::access<friended>::reachable(member(^^friended, "hidden")));
static_assert(not reflex::qt::access<unfriended>::reachable(member(^^unfriended, "twice")));
static_assert(not reflex::qt::access<unfriended>::reachable(member(^^unfriended, "hidden")));

QCoreApplication& application()
{
  static int              argc   = 1;
  static char             arg0[] = "reflex-test-qt-access";
  static char*            argv[] = {arg0, nullptr};
  static QCoreApplication instance(argc, argv);
  return instance;
}

template <typename Predicate> bool spin_until(Predicate done, std::chrono::milliseconds budget)
{
  QDeadlineTimer deadline(budget);
  while(not done())
  {
    if(deadline.hasExpired())
    {
      return false;
    }
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 10);
  }
  return true;
}
}

TEST_CASE("a member the friend declaration does not open is rejected by name")
{
  consteval
  {
    REFLEX_CONSTEVAL_NOTHROW(
        reflex::qt::access<friended>::require_reachable(member(^^friended, "twice")));
    REFLEX_CONSTEVAL_NOTHROW(
        reflex::qt::access<controller>::require_reachable(member(^^controller, "onThing")));

    REFLEX_CONSTEVAL_THROWS(
        reflex::qt::access<unfriended>::require_reachable(member(^^unfriended, "twice")));
    REFLEX_CONSTEVAL_THROWS(
        reflex::qt::access<unfriended>::require_reachable(member(^^unfriended, "hidden")));
  }
}

TEST_CASE("a private gadget member is published and invoked through the metaobject")
{
  const QMetaObject& mo = friended::staticMetaObject;

  REQUIRE(mo.propertyCount() == 1);
  CHECK(std::string_view{mo.property(0).name()} == "hidden");

  friended  f;
  const int index = mo.indexOfMethod("twice()");
  REQUIRE(index >= 0);

  int doubled = 0;
  REQUIRE(mo.method(index).invokeOnGadget(&f, qReturnArg(doubled)));
  CHECK(doubled == 6);

  CHECK(mo.property(0).readOnGadget(&f).toInt() == 3);
  REQUIRE(mo.property(0).writeOnGadget(&f, 10));
  CHECK(mo.property(0).readOnGadget(&f).toInt() == 10);
}

TEST_CASE("private slots, invocables and enums reach the method table")
{
  const QMetaObject& mo     = controller::staticMetaObject;
  const int          offset = mo.methodOffset();

  const QList<QByteArray> expected{"changed(int)", "countChanged()", "onThing(int)", "doubled()"};
  QList<QByteArray>       signatures;
  for(int i = offset; i < mo.methodCount(); ++i)
  {
    signatures.push_back(mo.method(i).methodSignature());
  }
  CHECK(signatures == expected);

  REQUIRE(mo.enumeratorCount() == 1);
  const QMetaEnum e = mo.enumerator(0);
  CHECK(std::string_view{e.name()} == "mode");
  REQUIRE(e.keyCount() == 2);
  CHECK(std::string_view{e.key(0)} == "idle");
  CHECK(std::string_view{e.key(1)} == "busy");
}

TEST_CASE("a private slot and a private invocable answer invokeMethod")
{
  controller c;

  REQUIRE(QMetaObject::invokeMethod(&c, "onThing", Q_ARG(int, 4)));
  CHECK(c.seen == 4);

  REQUIRE(c.setProperty("count", 10));

  int doubled = 0;
  REQUIRE(QMetaObject::invokeMethod(&c, "doubled", Q_RETURN_ARG(int, doubled)));
  CHECK(doubled == 10);
}

TEST_CASE("a private property reads and writes through its private accessors")
{
  controller c;
  int        notified = 0;

  QObject::connect(&c, &controller::propertyChanged<"count">, [&notified] { ++notified; });

  REQUIRE(c.setProperty("count", 20));
  CHECK(c.property("count").toInt() == 20);
  CHECK(c.property<"count">() == 20);
  CHECK(c.listened == 1);
  CHECK(notified == 1);

  c.setProperty<"count">(40);
  CHECK(c.property<"count">() == 40);
  CHECK(c.listened == 2);
  CHECK(notified == 2);
}

TEST_CASE("a private signal member connects and activates")
{
  controller c;
  int        seen = 0;

  c.on_changed([&seen](int n) { seen += n; });
  c.fire(5);
  CHECK(seen == 5);
}

TEST_CASE("a private timer handler is dispatched")
{
  application();

  controller c;
  REQUIRE(c.start() != 0);
  REQUIRE(spin_until([&c] { return c.ticks > 0; }, 2s));
  CHECK(c.stop());
}
