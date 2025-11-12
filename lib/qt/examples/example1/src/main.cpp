#include <QCommandLineOption>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>

#include <chrono>

using namespace reflex;

int main(int argc, char** argv)
{
  QGuiApplication app(argc, argv);
  app.setOrganizationName("reflex");
  app.setOrganizationDomain("examples");
  app.setApplicationName("example1");

  QQmlApplicationEngine engine;
  engine.loadFromModule("Example1", "App");
  return app.exec();
}