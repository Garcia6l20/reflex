#include <clock/types.hpp>

#include <QtCore/qtimer.h>
#include <QtGui/qguiapplication.h>
#include <QtQml/qqmlapplicationengine.h>

#include <cstdlib>
#include <print>
#include <string_view>

int main(int argc, char** argv)
{
  const bool checking = argc > 1 and std::string_view{argv[1]} == "--check";

  QGuiApplication       app(argc, argv);
  QQmlApplicationEngine engine;

  engine.loadFromModule("Reflex.Clock", checking ? "Check" : "Main");
  if(engine.rootObjects().isEmpty())
  {
    return EXIT_FAILURE;
  }

  if(not checking)
  {
    return QGuiApplication::exec();
  }

  auto* const clock = qobject_cast<clock_example::Clock*>(
      engine.rootObjects().constFirst()->property("clock").value<QObject*>());
  if(clock == nullptr)
  {
    return EXIT_FAILURE;
  }

  QObject::connect(&app, &QCoreApplication::aboutToQuit, clock, [clock] {
    std::println(
        "{} ticks {} observed {} note {}", clock->caption().toStdString(), clock->ticks,
        clock->observed(), clock->note().toStdString());
  });
  QTimer::singleShot(3000, &app, [] { QCoreApplication::exit(EXIT_FAILURE); });

  return QGuiApplication::exec();
}
