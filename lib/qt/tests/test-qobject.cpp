#define REFLEX_QT_INTERNAL_VISIBILITY public

#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>
#include <test_object.hpp>

#include <cmath>
#include <print>

using namespace reflex;

struct ml_object : qt::object<ml_object>
{
  signal<>    emptySig{this};
  signal<int> intSig{this};

  [[= slot]] void emptySlot()
  {
    std::println("ml_object: emptySlot called");
  }

  [[= slot]] void intSlot(int value)
  {
    std::println("ml_object: intSlot called: {}", value);
  }

  [[= slot]] void handleMessage(TestMessage const& value)
  {
    std::println("ml_object: TestMessage: {} ({})", value.body(), value.headers());
  }

  [[= slot]] void handleObject(QObject* obj)
  {
    qDebug() << "ml_object: handleObject:" << obj->objectName();
  }

  [[= slot]] void handleQmlEngine(QQmlEngine* engine)
  {
    qDebug() << "ml_object: handleQmlEngine:" << engine->objectName();
  }

  [[= invocable]] bool sayTheTruth(bool lie = false) // TODO handle defaulted arguments
  {
    std::println("ml_object: I'm{}lying !", lie ? " " : " not ");
    return lie;
  }

private:
  [[= prop<"rwn">]] int intProp = 42;

  [[= listener_of<^^intProp>]] void intPropListener()
  {
    std::println("ml_object: intProp is now {}", intProp);
  }

  [[= prop<"rwn">]] double      power = 42;
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
    std::println("ml_object: power is now {}", power);
  }
};

#define dump_exec(...)                \
  std::println("> {}", #__VA_ARGS__); \
  __VA_ARGS__

int main(int argc, char** argv)
{
  static_assert(qt::detail::static_meta_type_id_of(^^int) == QMetaType::Type::Int);
  static_assert(qt::detail::static_meta_type_id_of(^^QObject*) == QMetaType::Type::QObjectStar);
  static_assert(qt::detail::static_meta_type_id_of(^^QQmlEngine*) == qt::detail::custom_type);
  static_assert(qt::detail::static_meta_type_id_of(^^TestMessage) == qt::detail::custom_type);

  QTestObject to;
  ml_object   mlo;

  qt::dump(to);
  qt::dump(mlo);

  QObject::connect(&mlo, &ml_object::emptySig, &to, &QTestObject::emtpySlot);
  dump_exec(mlo.emptySig());
  QObject::connect(&mlo, &ml_object::intSig, &to, &QTestObject::intSlot);
  dump_exec(mlo.intSig(42));

  QObject::connect(&to, &QTestObject::emptySig, &mlo, &ml_object::emptySlot);
  dump_exec(to.emptySig());
  QObject::connect(&to, &QTestObject::intSig, &mlo, &ml_object::intSlot);
  dump_exec(to.intSig(42));
  {
    TestMessage m{"hello", QStringList{"h1", "h2"}};
    QMetaObject::invokeMethod(&to, "handleMessage", Q_ARG(TestMessage, m));
    QMetaObject::invokeMethod(&mlo, "handleMessage", Q_ARG(TestMessage, m));
  }

  QMetaObject::invokeMethod(&to, "intSlot", Q_ARG(int, 55));
  QMetaObject::invokeMethod(&mlo, "intSlot", Q_ARG(int, 55));
  bool truth = false;
  QMetaObject::invokeMethod(&mlo, "sayTheTruth", Q_RETURN_ARG(bool, truth)); // TODO handle default args
  QMetaObject::invokeMethod(&mlo, "sayTheTruth", Q_RETURN_ARG(bool, truth), Q_ARG(bool, true));
  QMetaObject::invokeMethod(&mlo, "sayTheTruth", Q_RETURN_ARG(bool, truth), Q_ARG(bool, false));

  {
    const auto value = mlo.property("intProp");
    mlo.setProperty("intProp", value.toInt() + 1);
    qDebug() << mlo.property("intProp");
  }
  {
    const auto value = mlo.property<"intProp">();
    mlo.setProperty<"intProp">(value + 1);
    qDebug() << mlo.property("intProp");
  }

  QObject::connect(&mlo,
                   &ml_object::propertyChanged<"intProp">,
                   [&mlo] { std::println("intProp change caught: {}", mlo.property<"intProp">()); });
  mlo.setProperty<"intProp">(12);

  mlo.setProperty<"power">(3);
  std::println("power: {} dB", mlo.property<"power">());
  mlo.setProperty<"power">(6);
  std::println("power: {} dB", mlo.property<"power">());
}
