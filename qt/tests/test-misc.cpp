#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QObject>

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
