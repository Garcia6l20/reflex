#define REFLEX_QT_INTERNAL_VISIBILITY public

#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>
#include <test_object.hpp>

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

  [[= invocable]] bool sayTheTruth()
  {
    std::println("ml_object: I'm not lying !");
    return true;
  }

private:
  [[= prop<"rwn">]] int intProp = 42;

  void intProp_written()
  {
    std::println("ml_object: intProp is now {}", intProp);
  }
};

#define dump_exec(...)                \
  std::println("> {}", #__VA_ARGS__); \
  __VA_ARGS__

int main(int argc, char** argv)
{
  // constexpr auto strings = ml_object::__get_strings(); // patch qt::object to make __get_strings public for debugging
  // template for(constexpr auto s : strings)
  // {
  //   using wrapper = typename[:s:];
  //   std::println("- \"{}\": \"{}\" ({} chars, {:p})", display_string_of(s),
  //                [:s:] ::view(),
  //                [:s:] ::size(), static_cast<const void*>(&[:s:] ::data));
  // }

  QTestObject to;
  // qt::dump(to);

  ml_object mlo;
  qt::dump(mlo);

  QObject::connect(&mlo, &ml_object::emptySig, &to, &QTestObject::emtpySlot);
  dump_exec(mlo.emptySig());
  QObject::connect(&mlo, &ml_object::intSig, &to, &QTestObject::intSlot);
  dump_exec(mlo.intSig(42));

  QObject::connect(&to, &QTestObject::emptySig, &mlo, &ml_object::emptySlot);
  dump_exec(to.emptySig());
  QObject::connect(&to, &QTestObject::intSig, &mlo, &ml_object::intSlot);
  dump_exec(to.intSig(42));

  // QMetaObject::invokeMethod(&to, "intSlot", Q_ARG(int, 55));
  QMetaObject::invokeMethod(&mlo, "intSlot", Q_ARG(int, 55));
  bool truth = false;
  QMetaObject::invokeMethod(&mlo, "sayTheTruth", Q_RETURN_ARG(bool, truth));

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
}
