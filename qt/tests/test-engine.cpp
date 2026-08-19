#include <engine/types.hpp>

#include <doctest/doctest.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlComponent>

#include <string>

/** @file
 *
 * What a real `QQmlApplicationEngine` accepts, which is neither what
 * `qmltyperegistrar` writes nor what `qmllint` reads: the engine builds a
 * property cache of its own and rejects shapes both tools call clean, without
 * printing anything. `rootObjects()` coming back empty is the whole diagnosis
 * it offers, so every case here reads `QQmlComponent::errors()` first and
 * carries the text into the failure.
 */
namespace
{
QCoreApplication& application()
{
  static int              argc   = 1;
  static char             arg0[] = "reflex-test-qt-engine";
  static char*            argv[] = {arg0, nullptr};
  static QCoreApplication instance(argc, argv);
  return instance;
}

std::string load_errors(QQmlEngine& engine)
{
  QQmlComponent component(&engine);
  component.loadFromModule("Reflex.EngineTest", "Main");

  QString joined;
  for(auto const& error : component.errors())
  {
    joined += error.toString() + u'\n';
  }
  return joined.toStdString();
}
}

TEST_CASE("the engine builds a root object out of a reflex QML module")
{
  application();

  QQmlApplicationEngine engine;
  const auto            errors = load_errors(engine);
  INFO("QQmlComponent::errors(): " << errors);
  CHECK(errors.empty());

  engine.loadFromModule("Reflex.EngineTest", "Main");
  REQUIRE_FALSE(engine.rootObjects().isEmpty());
}

TEST_CASE("a private signal binds its QML handler")
{
  application();

  QQmlApplicationEngine engine;
  const auto            errors = load_errors(engine);
  INFO("QQmlComponent::errors(): " << errors);

  engine.loadFromModule("Reflex.EngineTest", "Main");
  REQUIRE_FALSE(engine.rootObjects().isEmpty());

  CHECK(engine.rootObjects().constFirst()->property("heard").toInt() == 42);
}
