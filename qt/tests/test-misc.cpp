#include <doctest/doctest.h>

#include <reflex/qt.hpp>
#include <reflex/qt/debug.hpp>

#include <QtCore/QMetaClassInfo>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

struct pinger : reflex::qt::object<pinger>
{
  signal<> ping{this};
};

static_assert(not std::is_copy_constructible_v<reflex::qt::connection_guard>);
static_assert(not std::is_copy_assignable_v<reflex::qt::connection_guard>);
static_assert(std::is_nothrow_move_constructible_v<reflex::qt::connection_guard>);
static_assert(std::is_nothrow_move_assignable_v<reflex::qt::connection_guard>);

TEST_CASE("a connection_guard disconnects on scope exit")
{
  pinger sender;
  int    calls = 0;

  {
    reflex::qt::connection_guard guard =
        QObject::connect(&sender, &pinger::ping, [&calls] { ++calls; });
    CHECK(bool(guard));

    sender.ping();
    CHECK(calls == 1);
  }

  sender.ping();
  CHECK(calls == 1);
}

TEST_CASE("a released connection outlives its guard")
{
  pinger                  sender;
  int                     calls = 0;
  QMetaObject::Connection kept;

  {
    reflex::qt::connection_guard guard =
        QObject::connect(&sender, &pinger::ping, [&calls] { ++calls; });
    kept = guard.release();
    CHECK_FALSE(bool(guard));
  }

  sender.ping();
  CHECK(calls == 1);

  QObject::disconnect(kept);
  sender.ping();
  CHECK(calls == 1);
}

TEST_CASE("moving a connection_guard transfers ownership exactly once")
{
  pinger sender;
  int    calls = 0;

  {
    reflex::qt::connection_guard outer;
    {
      reflex::qt::connection_guard inner =
          QObject::connect(&sender, &pinger::ping, [&calls] { ++calls; });
      outer = std::move(inner);
      CHECK_FALSE(bool(inner));
      CHECK(bool(outer));
    }

    sender.ping();
    CHECK(calls == 1);
  }

  sender.ping();
  CHECK(calls == 1);
}

TEST_CASE("assigning over a connection_guard disconnects what it held")
{
  pinger sender;
  int    first  = 0;
  int    second = 0;

  reflex::qt::connection_guard guard =
      QObject::connect(&sender, &pinger::ping, [&first] { ++first; });
  guard = reflex::qt::connection_guard{
      QObject::connect(&sender, &pinger::ping, [&second] { ++second; })};

  sender.ping();
  CHECK(first == 0);
  CHECK(second == 1);

  guard.reset();
  CHECK_FALSE(bool(guard));

  sender.ping();
  CHECK(second == 1);
}

static_assert(std::formattable<QString, char>);
static_assert(not std::formattable<QString, wchar_t>,
              "the QString formatter is narrow only, on purpose");

TEST_CASE("a QString formats as UTF-8 through std::format")
{
  CHECK(std::format("{}", QString{"hello"}) == "hello");
  CHECK(std::format("{}", QString{}) == "");
  CHECK(std::format("{}", QString::fromUtf8("caf\xc3\xa9")) == "caf\xc3\xa9");
}

TEST_CASE("the QString formatter honours the string format spec")
{
  CHECK(std::format("{:>7}", QString{"hi"}) == "     hi");
  CHECK(std::format("{:.<7}", QString{"hi"}) == "hi.....");
  CHECK(std::format("{:.2}", QString{"hello"}) == "he");
}

struct [[= reflex::qt::classinfo{"author", "reflex"}]]
[[= reflex::qt::classinfo{"QML.Element", "Described"}]] described
    : reflex::qt::object<described>
{
  enum Color
  {
    Red,
    Green = 5
  };

  signal<int> valueChanged2{this};

  [[= slot]] void reset()
  {
    setProperty<"value">(0);
  }

  [[= invocable]] int twice() const
  {
    return 2 * value;
  }

  [[= prop{}]] int   value = 0;
  [[= prop{}]] Color color = Red;
};

TEST_CASE("describe lists what the metaobject declares")
{
  const std::string text = reflex::qt::describe<described>();

  CHECK(text.starts_with("class described\n"));
  CHECK(text.contains("  classinfo   author = reflex\n"));
  CHECK(text.contains("  classinfo   QML.Element = Described\n"));
  CHECK(text.contains("  signal      valueChanged2(int)\n"));
  CHECK(text.contains("  signal      valueChanged()\n"));
  CHECK(text.contains("  slot        reset()\n"));
  CHECK(text.contains("  method      twice()\n"));
  CHECK(text.contains("  property    value : int\n"));
  CHECK(text.contains("  property    color : described::Color\n"));
  CHECK(text.contains("  enum        Color\n"));
  CHECK(text.contains("    Red = 0\n"));
  CHECK(text.contains("    Green = 5\n"));
}

TEST_CASE("describe reads a real moc'ed metaobject too")
{
  QTimer     timer;
  const auto text = reflex::qt::describe(timer);

  CHECK(text.starts_with("class QTimer\n"));
  CHECK(text.contains("  signal      timeout()\n"));
  CHECK(text.contains("  slot        start()\n"));
  CHECK(text.contains("  property    active : bool\n"));
}

TEST_CASE("describe of an object reads its dynamic metaobject")
{
  described instance;
  CHECK(reflex::qt::describe(instance) == reflex::qt::describe<described>());
}

TEST_CASE("a class publishes every classinfo annotation it carries")
{
  const QMetaObject& meta = described::staticMetaObject;

  REQUIRE(meta.classInfoOffset() == 0);
  REQUIRE(meta.classInfoCount() == 2);

  CHECK(std::string_view{meta.classInfo(0).name()} == "author");
  CHECK(std::string_view{meta.classInfo(0).value()} == "reflex");
  CHECK(std::string_view{meta.classInfo(1).name()} == "QML.Element");
  CHECK(std::string_view{meta.classInfo(1).value()} == "Described");

  CHECK(meta.indexOfClassInfo("QML.Element") == 1);
  CHECK(meta.indexOfClassInfo("absent") == -1);
}

TEST_CASE("a class with no classinfo publishes none")
{
  CHECK(pinger::staticMetaObject.classInfoCount() == 0);
}
