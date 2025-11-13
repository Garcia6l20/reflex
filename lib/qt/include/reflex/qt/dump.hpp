#pragma once

#include <QDebug>
#include <QMetaClassInfo>
#include <QMetaMethod>
#include <QMetaObject>

#include <print>

namespace reflex::qt
{
inline void dump(QMetaObject const* const metaObject)
{
  qDebug() << "============= 📦" << metaObject->className() << "📦 =============";

  for(int i = 0; i < metaObject->classInfoCount(); ++i)
  {
    QMetaClassInfo info = metaObject->classInfo(i);
    qDebug().nospace() << " 💡       Info: " << info.name() << ": " << info.value();
  }

  // --- Signals/Slots/Invocables ---
  for(int i = 0; i < metaObject->methodCount(); ++i)
  {
    QMetaMethod method = metaObject->method(i);

    switch(method.methodType())
    {
      case QMetaMethod::Constructor:
        qDebug() << "🛠️ Constructor:" << method.methodSignature();
        break;
      case QMetaMethod::Signal:
        qDebug() << " 🔔     Signal:" << method.methodSignature();
        break;
      case QMetaMethod::Slot:
        qDebug() << " 🎯       Slot:" << method.methodSignature();
        break;
      case QMetaMethod::Method:
        qDebug() << " 📞  Invocable:" << method.methodSignature();
        break;
      default:
        // Ignore Method or Constructor types
        break;
    }
  }
  // --- Properties ---
  for(int i = metaObject->propertyOffset(); i < metaObject->propertyCount(); ++i)
  {
    QMetaProperty property = metaObject->property(i);
    qDebug() << " 🏷️    Property:" << property.name() << "-" << property.typeName();
  }
}

inline void dump(QObject const& object)
{
  dump(object.metaObject());
}

inline void dump(QObject* object)
{
  dump(*object);
}

template <typename T> inline void dump()
{
  dump(&T::staticMetaObject);
}

} // namespace reflex::qt