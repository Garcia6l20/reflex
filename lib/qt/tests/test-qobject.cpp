#include <reflex/qt.hpp>
#include <test_object.hpp>

#include <print>

using namespace reflex;

struct ml_object : qt::object<ml_object>
{
  [[= qt::signal]] inline void emptySig()
  {
    trigger<^^emptySig>();
  }

  [[= qt::signal]] inline void intSig(int value)
  {
    trigger<^^intSig>(value);
  }

  [[= qt::slot]] void emptySlot()
  {
    std::println("ml_object: emptySlot called");
  }

  [[= qt::slot]] void intSlot(int value)
  {
    std::println("ml_object: intSlot called: {}", value);
  }

  void not_a_signal()
  {
  }

};

#include <QCoreApplication>
#include <QDebug>
#include <QMetaClassInfo>
#include <QMetaMethod>
#include <QMetaObject>

void dumpObject(QObject* object)
{
  const QMetaObject* metaObject = object->metaObject();

  qDebug() << "Class:" << metaObject->className();

  for(int i = 0; i < metaObject->classInfoCount(); ++i)
  {
    QMetaClassInfo info = metaObject->classInfo(i);
    qDebug() << "  Info:" << info.name() << info.value();
  }

  for(int i = 0; i < metaObject->methodCount(); ++i)
  {
    QMetaMethod method = metaObject->method(i);

    switch(method.methodType())
    {
      case QMetaMethod::Signal:
        qDebug() << "  Signal:" << method.methodSignature();
        break;
      case QMetaMethod::Slot:
        qDebug() << "  Slot:" << method.methodSignature();
        break;
      default:
        // Ignore Method or Constructor types
        break;
    }
  }
}

int main(int argc, char** argv)
{
  QCoreApplication app{argc, argv};

  // constexpr auto strings = ml_object::__get_strings(); // patch qt::object to make __get_strings public for debugging
  // template for(constexpr auto s : strings)
  // {
  //   using wrapper = typename[:s:];
  //   std::println("- \"{}\": \"{}\" ({} chars, {:p})", display_string_of(s),
  //                [:s:] ::view(),
  //                [:s:] ::size(), static_cast<const void*>(&[:s:] ::data));
  // }

  QTestObject to;
  // dumpObject(&to);

  ml_object mlo;
  dumpObject(&mlo);

  QObject::connect(&mlo, &ml_object::emptySig, &to, &QTestObject::emtpySlot);
  mlo.emptySig();
  QObject::connect(&mlo, &ml_object::intSig, &to, &QTestObject::intSlot);
  mlo.intSig(42);

  QObject::connect(&to, &QTestObject::emptySig, &mlo, &ml_object::emptySlot);
  to.emptySig();
  QObject::connect(&to, &QTestObject::intSig, &mlo, &ml_object::intSlot);
  to.intSig(42);
}
