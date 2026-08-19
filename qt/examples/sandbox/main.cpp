#include <sandbox/types.hpp>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qtimer.h>
#include <QtQml/qqmlapplicationengine.h>

#include <reflex/qt/format.hpp>

#include <cstdlib>
#include <print>

int main(int argc, char** argv)
{
  QCoreApplication      app(argc, argv);
  QQmlApplicationEngine engine;

  engine.loadFromModule("Reflex.Sandbox", "Main");
  if(engine.rootObjects().isEmpty())
  {
    return EXIT_FAILURE;
  }

  auto* const box = qobject_cast<sandbox::controller*>(
      engine.rootObjects().constFirst()->property("sandbox").value<QObject*>());
  if(box == nullptr)
  {
    return EXIT_FAILURE;
  }

  QObject::connect(&app,
                   &QCoreApplication::aboutToQuit,
                   box,
                   [box] {
                     std::println("{} observed {} note {}",
                                  box->caption(),
                                  box->observed(),
                                  box->note());
                   });
  QTimer::singleShot(3000, &app, [] { QCoreApplication::exit(EXIT_FAILURE); });

  return QCoreApplication::exec();
}
