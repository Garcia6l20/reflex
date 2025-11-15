#include <QCommandLineOption>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <chrono>

int main(int argc, char** argv)
{
  QGuiApplication app(argc, argv);
  app.setOrganizationName("reflex");
  app.setOrganizationDomain("examples");
  app.setApplicationName("example1");

  qSetMessagePattern("[%{time mm:ss.zzz}][%{type}] %{category}: %{message}");

  QLoggingCategory::setFilterRules("qt.scenegraph.general=true\n"
                                   "qt.qml.*.debug=true\n"
                                   "qt.qml.overloadresolution.debug=false");

  QQmlApplicationEngine engine;
  engine.loadFromModule("Example1", "App");
  return app.exec();
}