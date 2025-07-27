#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest>

#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>

using namespace reflex;

class SimplePropertyController : public qt::object<SimplePropertyController>
{
public:
  using qt::object<SimplePropertyController>::object;

  [[= slot]] void incr()
  {
    setProperty<^^ivalue>(ivalue + 1);
  }

private:
  [[= prop<"rwn">]] int ivalue = 42;
};

class GetSetPropertyController : public qt::object<GetSetPropertyController>
{
public:
  using qt::object<GetSetPropertyController>::object;

private:
  [[= prop<"rwn">]] int          ivalue = 42;
  [[= setter_of<^^ivalue>]] void set(int value)
  {
    ivalue = value * 2;
  }
  [[= getter_of<^^ivalue>]] int get()
  {
    return ivalue * 2;
  }
};

class InvocableTestController : public qt::object<InvocableTestController>
{
public:
  using qt::object<InvocableTestController>::object;

  [[= invocable]] double mult(double a, double b)
  {
    return a * b;
  }
};

class SignalSlotController : public qt::object<SignalSlotController>
{
public:
  using qt::object<SignalSlotController>::object;

  signal<bool> toggled{this};

  [[= slot]] void trig()
  {
    toggled(value_);
    value_ = !value_;
  }

  signal<int, defaulted<int>> sig1{this, -1};
  signal<int, defaulted<int>> sig2{this, -1};

  [[= slot]] void slt1(int a = -1, int b = -2)
  {
    setProperty<^^slt1Value1>(a);
    setProperty<^^slt1Value2>(b);
  }

  [[= slot]] void slt2(int a = -1, int b = -2)
  {
    setProperty<^^slt2Value1>(a);
    setProperty<^^slt2Value2>(b);
  }

private:
  bool value_ = false;

  [[= prop<"rwn">]] int slt1Value1 = -1;
  [[= prop<"rwn">]] int slt1Value2 = -1;
  [[= prop<"rwn">]] int slt2Value1 = -1;
  [[= prop<"rwn">]] int slt2Value2 = -1;
};

class Setup : public qt::object<Setup>
{
public:
  Setup() = default;

  [[= slot]] void applicationAvailable()
  {
    // Initialization that only requires the QGuiApplication object to be available
  }

  [[= slot]] void qmlEngineAvailable(QQmlEngine* engine)
  {
    qt::dump(this);

    QLoggingCategory::setFilterRules(QStringLiteral("qt.qml.binding.removal.info=true"));

    // Initialization requiring the QQmlEngine to be constructed
    engine->rootContext()->setContextProperty("simplePropertyController", new SimplePropertyController(engine));
    engine->rootContext()->setContextProperty("invocableTestController", new InvocableTestController(engine));
    engine->rootContext()->setContextProperty("getSetPropertyController", new GetSetPropertyController(engine));

    engine->rootContext()->setContextProperty("signalSlotController", qt::dump(new SignalSlotController(engine)));
  }

  [[= slot]] void cleanupTestCase()
  {
    // Implement custom resource cleanup
  }
};

QUICK_TEST_MAIN_WITH_SETUP(qml - tests, Setup)
