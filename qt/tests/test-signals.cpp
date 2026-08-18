#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QTimer>

struct payload
{
  int value = 0;

  bool operator==(payload const&) const = default;
};

struct emitter : reflex::qt::object<emitter>
{
  signal<>                       ping{this};
  signal<int, with_default<int>> pair{this, 42};
  signal<payload>                boxed{this};

  [[= slot]] void onPing()
  {
    ++pings;
  }

  [[= slot]] void onPair(int a = -1, int b = -1)
  {
    ++pairs;
    last_a = a;
    last_b = b;
  }

  [[= slot]] void onBoxed(payload const& p)
  {
    ++boxes;
    last_payload = p;
  }

  [[= slot]] void onName(QString const& name)
  {
    last_name = name;
  }

  [[= prop{}]] int level = 0;

  int     pings  = 0;
  int     pairs  = 0;
  int     boxes  = 0;
  int     last_a = -1;
  int     last_b = -1;
  payload last_payload;
  QString last_name;
};

struct spread : reflex::qt::object<spread>
{
  signal<int, with_default<int>, with_default<int>> three{this, 20, 30};

  [[= slot]] void onThree(int a, int b, int c)
  {
    last = {a, b, c};
  }

  std::array<int, 3> last{};
};

namespace
{
QCoreApplication& application()
{
  static int              argc   = 1;
  static char             arg0[] = "reflex-test-qt-signals";
  static char*            argv[] = {arg0, nullptr};
  static QCoreApplication instance(argc, argv);
  return instance;
}
}

TEST_CASE("new-style connect, reflex signal to reflex slot")
{
  emitter sender;
  emitter receiver;

  QObject::connect(&sender, &emitter::ping, &receiver, &emitter::onPing);
  sender.ping();
  CHECK(receiver.pings == 1);

  QObject::connect(&sender, &emitter::pair, &receiver, &emitter::onPair);
  sender.pair(7, 8);
  CHECK(receiver.pairs == 1);
  CHECK(receiver.last_a == 7);
  CHECK(receiver.last_b == 8);

  sender.pair(9);
  CHECK(receiver.pairs == 2);
  CHECK(receiver.last_a == 9);
  CHECK(receiver.last_b == 42);
}

TEST_CASE("a signal with two defaults fills them from the right end")
{
  spread sender;
  spread receiver;

  QObject::connect(&sender, &spread::three, &receiver, &spread::onThree);

  sender.three(1, 2, 3);
  CHECK(receiver.last == std::array{1, 2, 3});

  sender.three(1, 2);
  CHECK(receiver.last == std::array{1, 2, 30});

  sender.three(1);
  CHECK(receiver.last == std::array{1, 20, 30});
}

TEST_CASE("new-style connect across the moc boundary")
{
  application();

  emitter reflex_side;
  QTimer  real_side;
  real_side.setInterval(100000);

  QObject::connect(&reflex_side, &emitter::ping, &real_side, qOverload<>(&QTimer::start));
  CHECK(not real_side.isActive());
  reflex_side.ping();
  CHECK(real_side.isActive());
  real_side.stop();

  QObject real_sender;
  QObject::connect(&real_sender, &QObject::objectNameChanged, &reflex_side, &emitter::onName);
  real_sender.setObjectName(QStringLiteral("hello"));
  CHECK(reflex_side.last_name == QStringLiteral("hello"));
}

TEST_CASE("string-based connect across the moc boundary")
{
  application();

  emitter reflex_side;
  QTimer  real_side;
  real_side.setInterval(100000);

  QObject::connect(&reflex_side, SIGNAL(ping()), &real_side, SLOT(start()));
  reflex_side.ping();
  CHECK(real_side.isActive());
  real_side.stop();

  QObject real_sender;
  QObject::connect(&real_sender, SIGNAL(objectNameChanged(QString)), &reflex_side, SLOT(onName(QString)));
  real_sender.setObjectName(QStringLiteral("world"));
  CHECK(reflex_side.last_name == QStringLiteral("world"));
}

TEST_CASE("connect to a lambda, and disconnect it again")
{
  emitter sender;
  int     seen = 0;

  const auto connection = QObject::connect(&sender, &emitter::ping, [&seen] { ++seen; });
  REQUIRE(connection);
  sender.ping();
  CHECK(seen == 1);

  CHECK(QObject::disconnect(connection));
  sender.ping();
  CHECK(seen == 1);
}

TEST_CASE("a property notifies through its generated signal")
{
  emitter sender;
  int     notified = 0;

  QObject::connect(&sender, &emitter::propertyChanged<"level">, &sender, [&notified] { ++notified; });

  sender.setProperty<"level">(3);
  CHECK(sender.level == 3);
  CHECK(notified == 1);

  sender.setProperty<"level">(3);
  CHECK(notified == 1);

  sender.setProperty("level", 4);
  CHECK(notified == 2);
}

TEST_CASE("a queued connection marshals a custom argument")
{
  static_assert(not QtPrivate::TypesAreDeclaredMetaType<QtPrivate::List<payload>>::Value,
                "the fallback below is only interesting while payload is not a declared metatype");

  application();

  emitter sender;
  emitter receiver;

  QObject::connect(&sender, &emitter::boxed, &receiver, &emitter::onBoxed, Qt::QueuedConnection);
  sender.boxed(payload{7});
  CHECK(receiver.boxes == 0);

  QCoreApplication::processEvents();
  CHECK(receiver.boxes == 1);
  CHECK(receiver.last_payload == payload{7});
}

TEST_CASE("a cross-thread connection marshals a custom argument")
{
  application();

  emitter  sender;
  QThread  worker;
  auto*    receiver = new emitter;
  receiver->moveToThread(&worker);

  QObject::connect(&sender, &emitter::boxed, receiver, &emitter::onBoxed);

  worker.start();
  sender.boxed(payload{11});
  QMetaObject::invokeMethod(receiver, [&worker] { worker.quit(); }, Qt::QueuedConnection);
  REQUIRE(worker.wait());

  CHECK(receiver->boxes == 1);
  CHECK(receiver->last_payload == payload{11});
  delete receiver;
}
