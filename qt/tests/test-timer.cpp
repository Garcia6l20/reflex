#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QObject>

#include <chrono>

using namespace std::chrono_literals;

struct baseline : QObject
{
  int value = 0;
};

struct timerless : reflex::qt::object<timerless>
{
  [[= prop{}]] int value = 0;
};

static_assert(
    sizeof(timerless) == sizeof(baseline),
    "an object with no timer event costs no more than a plain QObject");

struct ticker : reflex::qt::object<ticker>
{
  void tick()
  {
    ++ticks;
  }

  void tock()
  {
    ++tocks;
  }

  timer<^^tick> tick_timer;
  timer<^^tock> tock_timer;

  int ticks = 0;
  int tocks = 0;
};

static_assert(
    sizeof(reflex::qt::detail::timer_decl<^^ticker::tick>) == sizeof(int),
    "a timer costs one int");

struct base_ticker : reflex::qt::object<base_ticker>
{
  void beat()
  {
    ++beats;
  }

  timer<^^beat> beat_timer;

  int beats = 0;
};

struct derived_ticker : reflex::qt::object<derived_ticker, base_ticker>
{};

struct elsewhere
{
  void ping() {}
};

struct foreign_handler
{
  reflex::qt::detail::timer_decl<^^elsewhere::ping> t;
};

struct duplicate_handlers
{
  void beep() {}

  reflex::qt::detail::timer_decl<^^beep> a;
  reflex::qt::detail::timer_decl<^^beep> b;
};

namespace timer_meta = reflex::qt::detail;

static_assert(
    timer_meta::timer_handler_of(
        reflex::meta::member_named(^^ticker, "tick_timer", reflex::meta::access_context::unchecked()))
        == ^^ticker::tick,
    "a handler written in the class body is the same entity as one written outside");

static_assert(timer_meta::timer_handlers_are_reachable(^^ticker));
static_assert(timer_meta::timer_handlers_are_unique(^^ticker));
static_assert(not timer_meta::timer_handlers_are_reachable(^^foreign_handler));
static_assert(not timer_meta::timer_handlers_are_unique(^^duplicate_handlers));

namespace
{
QCoreApplication& application()
{
  static int              argc   = 1;
  static char             arg0[] = "reflex-test-qt-timer";
  static char*            argv[] = {arg0, nullptr};
  static QCoreApplication instance(argc, argv);
  return instance;
}

/** @brief spins the event loop until @p done or the deadline, without sleeping
 *
 * @return `false` when the deadline is what ended the wait, which every caller
 *         asserts against so that a silently never-firing timer fails loudly.
 */
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

TEST_CASE("a timer event fires and stops firing once killed")
{
  application();

  ticker t;
  CHECK_FALSE(t.tick_timer.isActive());

  const int id = t.startTimer<^^ticker::tick>(1);
  CHECK(id != 0);
  CHECK(t.tick_timer.id() == id);
  CHECK(t.tick_timer.isActive());

  REQUIRE(spin_until([&t] { return t.ticks > 0; }, 2s));
  CHECK(t.tocks == 0);

  CHECK(t.killTimer<^^ticker::tick>());
  CHECK_FALSE(t.tick_timer.isActive());
  CHECK(t.tick_timer.id() == 0);

  const int seen = t.ticks;
  CHECK_FALSE(spin_until([&t, seen] { return t.ticks > seen; }, 100ms));
  CHECK(t.ticks == seen);
}

TEST_CASE("two timer events on one class fire independently")
{
  application();

  ticker t;
  CHECK(t.startTimer<^^ticker::tick>(1) != 0);
  CHECK(t.startTimer<^^ticker::tock>(1) != 0);
  CHECK(t.tick_timer.id() != t.tock_timer.id());

  REQUIRE(spin_until([&t] { return t.ticks > 0 and t.tocks > 0; }, 2s));

  CHECK(t.killTimer<^^ticker::tick>());
  const int ticks = t.ticks;
  const int tocks = t.tocks;

  REQUIRE(spin_until([&t, tocks] { return t.tocks > tocks; }, 2s));
  CHECK(t.ticks == ticks);
}

TEST_CASE("starting a running timer reports through the return value")
{
  application();

  ticker    t;
  const int id = t.startTimer<^^ticker::tick>(1000);
  CHECK(id != 0);
  CHECK(t.startTimer<^^ticker::tick>(1000) == 0);
  CHECK(t.tick_timer.id() == id);

  CHECK(t.killTimer<^^ticker::tick>());
  CHECK_FALSE(t.killTimer<^^ticker::tick>());
}

TEST_CASE("a derived object drives a timer its base declares")
{
  application();

  derived_ticker t;
  CHECK(t.startTimer<^^base_ticker::beat>(1) != 0);
  CHECK(t.beat_timer.isActive());

  REQUIRE(spin_until([&t] { return t.beats > 0; }, 2s));

  CHECK(t.killTimer<^^base_ticker::beat>());
}

TEST_CASE("the inherited startTimer and killTimer overloads still resolve")
{
  application();

  ticker    t;
  const int id = t.startTimer(1000);
  CHECK(id != 0);
  CHECK(t.startTimer<^^ticker::tick>(1000) != 0);
  CHECK(t.tick_timer.id() != id);

  t.killTimer(id);
  CHECK(t.killTimer<^^ticker::tick>());
}
