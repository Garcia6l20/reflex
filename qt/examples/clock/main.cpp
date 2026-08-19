#include <clock/types.hpp>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qtimer.h>
#include <QtQml/qqmlapplicationengine.h>

#include <cstdlib>
#include <print>

int main(int argc, char** argv)
{
  QCoreApplication      app(argc, argv);
  QQmlApplicationEngine engine;

  engine.loadFromModule("Reflex.Clock", "Main");
  if(engine.rootObjects().isEmpty())
  {
    return EXIT_FAILURE;
  }

  auto* const clock = qobject_cast<clock_example::Clock*>(
      engine.rootObjects().constFirst()->property("clock").value<QObject*>());
  if(clock == nullptr)
  {
    return EXIT_FAILURE;
  }

  QObject::connect(&app,
                   &QCoreApplication::aboutToQuit,
                   clock,
                   [clock] { std::println("observed {} label updates", clock->observed()); });
  QTimer::singleShot(3000, &app, [] { QCoreApplication::exit(EXIT_FAILURE); });

  return QCoreApplication::exec();
}
