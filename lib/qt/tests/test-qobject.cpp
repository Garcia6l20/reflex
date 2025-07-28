#include <reflex/qt.hpp>
#include <reflex/qt/connection_guard.hpp>
#include <reflex/qt/dump.hpp>
#include <test_object.hpp>

#include <cmath>
#include <print>

using namespace reflex;

struct ml_object : qt::object<ml_object>
{
  signal<>                    emptySig{this};
  signal<int, defaulted<int>> intSig{this, 42};
  // signal<int, defaulted<int>> intSig{this}; // Missing default argument(s)
  // signal<int, defaulted<int>> intSig{this, 42, 43}; // Too much default argument(s)

  [[= slot]] void emptySlot()
  {
    std::println("ml_object: emptySlot called");
  }

  [[= slot]] void intSlot(int value = 0, int value2 = 1)
  {
    std::println("ml_object: intSlot called: {} and {}", value, value2);
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

  [[= slot]] void qmlEngineAvailable(QQmlEngine* engine)
  {
  }

  [[= invocable]] bool sayTheTruth(bool lie = false)
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
  // static_assert(qt::detail::static_meta_type_id_of(^^QQmlEngine*) == qt::detail::custom_type);
  static_assert(qt::detail::static_meta_type_id_of(^^TestMessage) == qt::detail::custom_type);

  // template for(constexpr auto T : ml_object::__custom_types())
  // {
  //   std::println("{}", display_string_of(T));
  // }

  // static constexpr auto data = ml_object::__introspection_data();

  // template for(constexpr auto ii : std::views::iota(size_t(0), std::tuple_size_v<decltype(data.strings)>))
  // {
  //   constexpr auto s    = std::get<ii>(data.strings);
  //   constexpr auto size = s.size();
  //   std::println("{} (size={}, offset={})", s.view(), size, ii);
  // }
  // template for(constexpr auto R : std::array{^^TestMessage const&, ^^QQmlEngine* })
  // {
  //   // constexpr auto id = []
  //   // {
  //   //   if(has_identifier(R))
  //   //   {
  //   //     return identifier_of(R);
  //   //   }
  //   //   else if(is_type(R))
  //   //   {
  //   //     return display_string_of(remove_const(remove_reference(R)));
  //   //   }
  //   //   std::unreachable();
  //   // }();
  //   // std::print("{} identifier: {}...", display_string_of(R), id);
  //   // bool found = false;
  //   // template for(constexpr auto ii : std::views::iota(size_t(0), std::tuple_size_v<decltype(data.strings)>))
  //   // {
  //   //   if(id == std::get<ii>(data.strings).view())
  //   //   {
  //   //     std::println(" found at offset {} !", ii);
  //   //     found = true;
  //   //   }
  //   // }
  //   // if(not found)
  //   // {
  //   //   std::println(" not found !");
  //   // }
  //   constexpr auto offset = data.template custom_type_offset_of<R>();
  //   constexpr auto str    = data.template string_view_at<offset>();
  //   std::println("{} at {}: {}", display_string_of(R), offset, str);
  // }

  QTestObject to;
  ml_object   mlo;

  qt::dump(to);
  qt::dump(mlo);

  QObject::connect(&mlo, &ml_object::emptySig, &to, &QTestObject::emtpySlot);
  dump_exec(mlo.emptySig());
  {
    std::println("======= lambda style connection =======");
    qt::connection_guard con = QObject::connect(&mlo, &ml_object::intSig, &to, &QTestObject::intSlot);
    dump_exec(mlo.intSig(42); mlo.intSig(43); mlo.intSig(444, 555););
  }
  {
    std::println("===== old school style connection (2 args) [MLO -> TO] =====");
    qt::connection_guard con = QObject::connect(&mlo, SIGNAL(intSig(int, int)), &to, SLOT(intSlot(int, int)));
    dump_exec(mlo.intSig(42));
    dump_exec(mlo.intSig(43));
  }
  {
    std::println("===== old school style connection (1 arg) [MLO -> TO] =====");
    qt::connection_guard con = QObject::connect(&mlo, SIGNAL(intSig(int)), &to, SLOT(intSlot(int)));
    dump_exec(mlo.intSig(42));
    dump_exec(mlo.intSig(43));
  }
  {
    std::println("===== old school style connection (2 args) [TO -> MLO] =====");
    qt::connection_guard con = QObject::connect(&to, SIGNAL(intSig(int, int)), &mlo, SLOT(intSlot(int, int)));
    dump_exec(to.intSig(42));
    dump_exec(to.intSig(43));
  }
  {
    std::println("===== old school style connection (1 args) [TO -> MLO] =====");
    qt::connection_guard con = QObject::connect(&to, SIGNAL(intSig(int)), &mlo, SLOT(intSlot(int)));
    dump_exec(to.intSig(42));
    dump_exec(to.intSig(43));
  }
  // mlo.intSig(); // Missing required signal argument(s)
  // mlo.intSig(1, 2, 3); // Too much argument(s)

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
  QMetaObject::invokeMethod(&mlo, "sayTheTruth", Q_RETURN_ARG(bool, truth));
  QMetaObject::invokeMethod(&mlo, "sayTheTruth", Q_RETURN_ARG(bool, truth), Q_ARG(bool, true));
  QMetaObject::invokeMethod(&mlo, "sayTheTruth", Q_RETURN_ARG(bool, truth), Q_ARG(bool, false));

  {
    const auto value = mlo.property("intProp");
    dump_exec(mlo.setProperty("intProp", value.toInt() + 1));
    qDebug() << mlo.property("intProp");
  }
  {
    const auto value = mlo.property<"intProp">();
    dump_exec(mlo.setProperty<"intProp">(value + 1));
    qDebug() << mlo.property("intProp");
  }

  QObject::connect(&mlo,
                   &ml_object::propertyChanged<"intProp">,
                   [&mlo] { std::println("intProp change caught: {}", mlo.property<"intProp">()); });
  dump_exec(mlo.setProperty<"intProp">(12));

  dump_exec(mlo.setProperty<"power">(3));
  std::println("power: {} dB", mlo.property<"power">());
  dump_exec(mlo.setProperty<"power">(6));
  std::println("power: {} dB", mlo.property<"power">());
}
