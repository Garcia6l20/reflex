#include <QCommandLineOption>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>

#include <chrono>

#include <Example.hpp>

using namespace reflex;

template <typename T> QVariant readGadgetProperty(const T& gadget, const char* name)
{
  const QMetaObject& mo  = T::staticMetaObject;
  int                idx = mo.indexOfProperty(name);
  if(idx < 0)
    return {};

  QMetaProperty prop  = mo.property(idx);
  QVariant      value = prop.readOnGadget(&gadget);
  return value;
}

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

  Message m{.subject = "hello", .body = "world"};
  qDebug() << readGadgetProperty(m, "subject");
  qDebug() << readGadgetProperty(m, "body");

  QQmlApplicationEngine engine;
  engine.loadFromModule("Example1Qt", "App");
  return app.exec();
}