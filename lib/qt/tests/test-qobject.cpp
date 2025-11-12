#include <reflex/qt.hpp>
#include <reflex/qt/connection_guard.hpp>
#include <reflex/qt/dump.hpp>
#include <ref_object.hpp>
#include <test_object.hpp>

#include <cmath>
#include <print>


#define dump_exec(...)                \
  std::println("> {}", #__VA_ARGS__); \
  __VA_ARGS__

#include <bit>

int main(int argc, char** argv)
{
  static_assert(qt::detail::static_meta_type_id_of(^^int) == QMetaType::Type::Int);
  static_assert(qt::detail::static_meta_type_id_of(^^QObject*) == QMetaType::Type::QObjectStar);
  // static_assert(qt::detail::static_meta_type_id_of(^^QQmlEngine*) == qt::detail::custom_type);
  static_assert(qt::detail::static_meta_type_id_of(^^TestMessage) == qt::detail::custom_type);

  // template for(constexpr auto S : test_object::__strings<test_object::tag>)
  // {
  //   std::println("{}", *S);
  // }

  QTestObject to;
  test_object   mlo;

  qt::dump(to);
  qt::dump(mlo);

  QObject::connect(&mlo, &test_object::emptySig, &to, &QTestObject::emtpySlot);
  dump_exec(mlo.emptySig());
  {
    std::println("======= lambda style connection =======");
    qt::connection_guard con = QObject::connect(&mlo, &test_object::intSig, &to, &QTestObject::intSlot);
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

  QObject::connect(&to, &QTestObject::emptySig, &mlo, &test_object::emptySlot);
  dump_exec(to.emptySig());
  QObject::connect(&to, &QTestObject::intSig, &mlo, &test_object::intSlot);
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
                   &test_object::propertyChanged<"intProp">,
                   [&mlo] { std::println("intProp change caught: {}", mlo.property<"intProp">()); });
  dump_exec(mlo.setProperty<"intProp">(12));

  dump_exec(mlo.setProperty<"power">(3));
  std::println("power: {} dB", mlo.property<"power">());
  dump_exec(mlo.setProperty<"power">(6));
  std::println("power: {} dB", mlo.property<"power">());
}
