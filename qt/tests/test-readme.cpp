#include <doctest/doctest.h>

#include <reflex/qt.hpp>
#include <reflex/qt/debug.hpp>
#include <reflex/qt/moc/export.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QFlags>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <source_location>
#include <string>
#include <string_view>

namespace qt = reflex::qt;

using namespace std::chrono_literals;

struct counter : qt::object<counter>
{
  signal<int> changed{this};

  [[= qt::slot]] void bump()
  {
    setProperty<"value">(value + 1);
    changed(value);
  }

  [[= qt::prop{}]] int value = 0;
};

namespace app
{
struct [[= qt::qml{}]] controller : qt::object<controller>
{
  [[= qt::prop{}]] int count = 0;
};

struct settings : qt::object<settings>
{
  [[= qt::prop{}]] int level = 0;
};
} // namespace app

REFLEX_QT_MODULE(app_types, m)
{
  m.expose<^^app>();
}

struct base_widget : qt::object<base_widget>
{
  [[= qt::prop{}]] int level = 0;
};

struct derived_widget : qt::object<derived_widget, base_widget>
{
  [[= qt::prop{}]] int depth = 0;
};

struct emitter : qt::object<emitter>
{
  signal<>                        ping{this};
  signal<int, qt::defaulted<int>> pair{this, 42};

  [[= qt::slot]] void onPair(int a, int b)
  {
    sum = a + b;
  }

  int sum = 0;
};

struct service : qt::object<service>
{
  [[= qt::slot]] void reset()
  {
    calls = 0;
  }

  [[= qt::invocable]] int twice(int n) const
  {
    return 2 * n;
  }

  int calls = 0;
};

struct settings : qt::object<settings>
{
  [[= qt::prop{}]] int                                volume = 0;
  [[= qt::prop{.write = false}]] int                  peak   = 0;
  [[= qt::prop{.notify = false}]] int                 cursor = 0;
  [[= qt::prop{.constant = true}]] int                limit  = 100;
  [[= qt::prop{.final = true, .required = true}]] int rate   = 44100;
};

struct scaled : qt::object<scaled>
{
  [[= qt::prop{}]] int raw = 0;

  [[= qt::getter<^^raw>]] int getRaw() const
  {
    return raw * 2;
  }

  [[= qt::setter<^^raw>]] void setRaw(int value)
  {
    raw = value / 2;
  }

  [[= qt::listener<^^raw>]] void onRawChanged()
  {
    ++changes;
  }

  int changes = 0;
};

struct[[= qt::naming::qt_style]] conventional : qt::object<conventional>
{
  [[= qt::prop{}]] int p1 = 0;

  int getP1() const
  {
    return p1 * 3;
  }

  void setP1(int value)
  {
    p1 = value / 3;
  }

  void onP1Changed()
  {
    ++changes;
  }

  int changes = 0;
};

struct poller : qt::object<poller>
{
  void tick()
  {
    ++ticks;
  }

  qt::timer<^^tick> tick_timer;

  int ticks = 0;
};

struct driver : qt::object<driver>
{
  int start()
  {
    return startTimer<^^tick>(50);
  }

  bool stop()
  {
    return killTimer<^^tick>();
  }

  void tick()
  {
    ++ticks;
  }

  qt::timer<^^tick> tick_timer;

  int ticks = 0;
};

struct point : qt::gadget<point>
{
  [[= qt::prop{}]] int x = 0;
  [[= qt::prop{}]] int y = 0;

  [[= qt::invocable]] int manhattan() const
  {
    return x + y;
  }
};

struct styled : qt::object<styled>
{
  enum Color
  {
    Red,
    Green = 5,
    Blue
  };

  enum class Mode
  {
    Fast,
    Slow = 9
  };

  enum Option
  {
    NoOption = 0x0,
    First    = 0x1,
    Second   = 0x2
  };

  using Options = QFlags<Option>;

  [[= qt::prop{}]] Color   color = Red;
  [[= qt::prop{}]] Mode    mode  = Mode::Fast;
  [[= qt::prop{}]] Options options;
};

struct controller : qt::object<controller>
{
  friend qt::access<controller>;

  int seen = 0;

private:
  [[= qt::slot]] void onThing(int n)
  {
    seen += n;
  }

  [[= qt::prop{}]] int count = 0;
};

struct [[= qt::classinfo{"author", "reflex"}]] described : qt::object<described>
{
  [[= qt::prop{}]] int value = 0;
};

namespace
{
QCoreApplication& application()
{
  static int              argc   = 1;
  static char             arg0[] = "reflex-test-qt-readme";
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

TEST_CASE("README: quick start")
{
  counter c;
  int     seen = 0;

  QObject::connect(&c, &counter::changed, [&seen](int n) { seen = n; });
  QMetaObject::invokeMethod(&c, "bump");

  CHECK(c.value == 1);
  CHECK(seen == 1);
}

TEST_CASE("README: a reflex class derives from another reflex class")
{
  const QMetaObject& child = derived_widget::staticMetaObject;

  CHECK(child.superClass() == &base_widget::staticMetaObject);
  CHECK(base_widget::staticMetaObject.superClass() == &QObject::staticMetaObject);

  derived_widget w;
  CHECK(w.setProperty("level", 3));
  CHECK(w.level == 3);
  CHECK(w.depth == 0);
}

TEST_CASE("README: signals")
{
  emitter e;
  QObject::connect(&e, &emitter::pair, &e, &emitter::onPair);

  e.pair(1, 2);
  CHECK(e.sum == 3);

  e.pair(1);
  CHECK(e.sum == 43);

  CHECK(emitter::staticMetaObject.indexOfSignal("pair(int,int)") >= 0);
  CHECK(emitter::staticMetaObject.indexOfSignal("pair(int)") >= 0);
}

TEST_CASE("README: slots and invocables")
{
  service s;
  int     doubled = 0;

  QMetaObject::invokeMethod(&s, "twice", Q_RETURN_ARG(int, doubled), Q_ARG(int, 21));
  CHECK(doubled == 42);

  CHECK(QMetaObject::invokeMethod(&s, "reset"));
  CHECK(s.calls == 0);
}

TEST_CASE("README: properties")
{
  settings s;

  s.setProperty<"volume">(7);
  const int typed = s.property<"volume">();

  s.setProperty("volume", 9);
  const QVariant boxed = s.property("volume");

  CHECK(typed == 7);
  CHECK(boxed.toInt() == 9);
}

TEST_CASE("README: the property flag table says what it does")
{
  const QMetaObject& mo     = settings::staticMetaObject;
  const int          offset = mo.propertyOffset();

  const QMetaProperty volume = mo.property(offset + 0);
  CHECK(volume.isReadable());
  CHECK(volume.isWritable());
  CHECK(volume.hasNotifySignal());

  CHECK(not mo.property(offset + 1).isWritable());
  CHECK(not mo.property(offset + 2).hasNotifySignal());

  const QMetaProperty limit = mo.property(offset + 3);
  CHECK(limit.isConstant());
  CHECK(not limit.isWritable());
  CHECK(not limit.hasNotifySignal());

  const QMetaProperty rate = mo.property(offset + 4);
  CHECK(rate.isFinal());
  CHECK(rate.isRequired());
}

TEST_CASE("README: accessors")
{
  scaled s;

  s.setProperty<"raw">(10);
  CHECK(s.raw == 5);
  CHECK(s.property<"raw">() == 10);
  CHECK(s.changes == 1);
}

TEST_CASE("README: naming conventions")
{
  conventional c;

  c.setProperty<"p1">(9);
  CHECK(c.p1 == 3);
  CHECK(c.property<"p1">() == 9);
  CHECK(c.changes == 1);
}

TEST_CASE("README: notify signals")
{
  counter c;
  int     notifications = 0;

  QObject::connect(&c, &counter::propertyChanged<"value">, [&notifications] { ++notifications; });
  c.setProperty<"value">(3);

  CHECK(notifications == 1);
}

TEST_CASE("README: timers")
{
  application();

  poller    p;
  const int id = p.startTimer<^^poller::tick>(50);
  CHECK(id != 0);
  CHECK(p.tick_timer.isActive());
  CHECK(p.tick_timer.id() == id);

  REQUIRE(spin_until([&p] { return p.ticks > 0; }, 2s));

  CHECK(p.killTimer<^^poller::tick>());
}

TEST_CASE("README: a class drives its own timer with the short spelling")
{
  application();

  driver d;
  CHECK(d.start() != 0);
  CHECK(d.tick_timer.isActive());

  REQUIRE(spin_until([&d] { return d.ticks > 0; }, 2s));

  CHECK(d.stop());
  CHECK_FALSE(d.tick_timer.isActive());
}

TEST_CASE("README: gadgets")
{
  point p;
  CHECK(point::staticMetaObject.property(0).writeOnGadget(&p, 3));
  CHECK(p.x == 3);

  const QVariant boxed = QVariant::fromValue(p);
  CHECK(boxed.metaType().flags().testFlag(QMetaType::IsGadget));

  CHECK(std::string_view{QMetaType::fromType<point>().name()} == "point");
  CHECK(QMetaType::fromType<point*>().flags().testFlag(QMetaType::PointerToGadget));
}

TEST_CASE("README: enums and flags")
{
  const QMetaObject& mo = styled::staticMetaObject;

  REQUIRE(mo.enumeratorCount() == 4);
  CHECK(std::string_view{mo.enumerator(0).name()} == "Color");
  CHECK(mo.enumerator(1).isScoped());
  CHECK(mo.enumerator(3).isFlag());

  const QMetaProperty color = mo.property(mo.propertyOffset() + 0);
  CHECK(color.isEnumType());
  CHECK(std::string_view{color.typeName()} == "styled::Color");
}

TEST_CASE("README: private members")
{
  controller c;

  REQUIRE(QMetaObject::invokeMethod(&c, "onThing", Q_ARG(int, 4)));
  CHECK(c.seen == 4);
  CHECK(c.setProperty("count", 2));
  CHECK(c.property("count").toInt() == 2);
}

TEST_CASE("README: class infos")
{
  const QMetaObject& mo = described::staticMetaObject;

  REQUIRE(mo.classInfoCount() == 1);
  CHECK(std::string_view{mo.classInfo(0).name()} == "author");
  CHECK(std::string_view{mo.classInfo(0).value()} == "reflex");
}

TEST_CASE("README: connection_guard")
{
  emitter sender;
  int     calls = 0;

  {
    qt::connection_guard guard = QObject::connect(&sender, &emitter::ping, [&calls] { ++calls; });
    sender.ping();
  }
  sender.ping();

  CHECK(calls == 1);
}

TEST_CASE("README: QString formatting")
{
  CHECK(std::format("{}", QString{"hello"}) == "hello");
  CHECK(std::format("{:>7}", QString{"hi"}) == "     hi");
  CHECK(std::format("{:.2}", QString{"hello"}) == "he");
}

template <typename F> static auto on_stdout(F&& produce) -> std::string
{
  const auto path  = std::filesystem::temp_directory_path() / "reflex-qt-dump.txt";
  const int  saved = ::dup(::fileno(stdout));
  std::fflush(stdout);
  REQUIRE(std::freopen(path.string().c_str(), "w", stdout) != nullptr);
  produce();
  std::fflush(stdout);
  ::dup2(saved, ::fileno(stdout));
  ::close(saved);

  std::ifstream in{path};
  std::string   text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  in.close();
  std::filesystem::remove(path);
  return text;
}

TEST_CASE("README: describe and dump")
{
  const std::string text = qt::describe<counter>();

  CHECK(text.starts_with("class counter\n"));
  CHECK(text.contains("  signal      changed(int)\n"));
  CHECK(text.contains("  signal      valueChanged()\n"));
  CHECK(text.contains("  slot        bump()\n"));
  CHECK(text.contains("  property    value : int\n"));

  counter instance;

  CHECK(on_stdout([] { qt::dump<counter>(); }) == text);
  CHECK(on_stdout([&instance] { qt::dump(instance); }) == text);
  CHECK(on_stdout([] { qt::dump(counter::staticMetaObject); }) == text);
}

TEST_CASE("README: a chained signal disconnects through its connection")
{
  emitter sender;
  emitter receiver;

  QObject::connect(&receiver, &emitter::pair, &receiver, &emitter::onPair);

  const auto chain = QObject::connect(&sender, &emitter::pair, &receiver, &emitter::pair);
  CHECK(not QObject::disconnect(&sender, &emitter::pair, &receiver, &emitter::pair));
  CHECK(QObject::disconnect(chain));

  sender.pair(1, 2);
  CHECK(receiver.sum == 0);
}

TEST_CASE("README: a module body publishes what the metatypes document describes")
{
  const auto document = reflex::qt::moc::metatypes_of<app_types>();

  REQUIRE(document.size() == 1);
  REQUIRE(document.front().classes.size() == 2);

  auto const& controller = document.front().classes.front();
  CHECK(controller.qualifiedClassName == "app::controller");
  REQUIRE(controller.classInfos.size() == 1);
  CHECK(controller.classInfos.front().name == "QML.Element");
  CHECK(controller.classInfos.front().value == "auto");

  CHECK(document.front().classes.back().qualifiedClassName == "app::settings");
}

TEST_CASE("README: include_roots shortens inputFile to an angled include")
{
  const auto here = std::filesystem::weakly_canonical(
      std::filesystem::path{std::source_location::current().file_name()});
  const auto full = reflex::qt::moc::metatypes_of<app_types>().front().inputFile;

  CHECK(full == here.generic_string());
  CHECK(reflex::qt::moc::metatypes_of<app_types>({{here.parent_path()}}).front().inputFile
        == "test-readme.cpp");
  CHECK(reflex::qt::moc::metatypes_of<app_types>({{here.parent_path().parent_path()}})
            .front()
            .inputFile
        == "tests/test-readme.cpp");
  CHECK(reflex::qt::moc::metatypes_of<app_types>(
            {{here.parent_path().parent_path().parent_path()}})
            .front()
            .inputFile
        == "qt/tests/test-readme.cpp");
}
