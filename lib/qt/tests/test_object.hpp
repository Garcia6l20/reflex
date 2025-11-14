#pragma once

#include <reflex/qt.hpp>
#include <ref_object.hpp>

#include <print>

using namespace reflex;

struct test_object : qt::object<test_object>
{
  signal<>                    emptySig{this};
  signal<int, defaulted<int>> intSig{this, 42};
  // signal<int, defaulted<int>> intSig{this}; // Missing default argument(s)
  // signal<int, defaulted<int>> intSig{this, 42, 43}; // Too much default argument(s)

  [[= slot]] void emptySlot()
  {
    std::println("test_object: emptySlot called");
  }

  [[= slot]] void intSlot(int value = 0, int value2 = 1)
  {
    std::println("test_object: intSlot called: {} and {}", value, value2);
  }

  [[= slot]] void handleMessage(TestMessage const& value)
  {
    std::println("test_object: TestMessage: {} ({})", value.body(), value.headers());
  }

  [[= slot]] void handleObject(QObject* obj)
  {
    qDebug() << "test_object: handleObject:" << obj->objectName();
  }

  [[= slot]] void handleQmlEngine(QQmlEngine* engine)
  {
    qDebug() << "test_object: handleQmlEngine:" << engine->objectName();
  }

  [[= slot]] void qmlEngineAvailable(QQmlEngine* engine)
  {
  }

  [[= invocable]] bool sayTheTruth(bool lie = false)
  {
    std::println("test_object: I'm{}lying !", lie ? " " : " not ");
    return lie;
  }

private:
  [[= prop{"rwn"}]] int intProp = 42;

  [[= listener_of<^^intProp>]] void intPropListener()
  {
    std::println("test_object: intProp is now {}", intProp);
  }

  [[= prop{"rwn"}]] double      power = 42;
  [[= setter_of<^^power>]] void setPower(double dB)
  {
    power = std::pow(10.0, dB / 10.0);
  }
  [[= getter_of<^^power>]] double getPower()
  {
    return 10.0 * std::log10(power);
  }
  [[= listener_of<^^power>]] void powerListener()
  {
    std::println("test_object: power is now {}", power);
  }
};
