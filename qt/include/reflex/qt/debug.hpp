#pragma once

#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>

#include <format>
#include <print>
#include <string>

namespace reflex::qt
{
namespace detail
{
constexpr const char* method_kind_name(QMetaMethod::MethodType type) noexcept
{
  switch(type)
  {
  case QMetaMethod::Constructor:
    return "constructor";
  case QMetaMethod::Signal:
    return "signal";
  case QMetaMethod::Slot:
    return "slot";
  case QMetaMethod::Method:
    return "method";
  }
  return "method";
}
}

/** @brief renders the class infos, methods, properties and enums of @p meta_object
 *
 * The walk is a runtime one over `QMetaObject`, so it reads a reflex class and
 * a real moc'ed one the same way. Only the entries @p meta_object declares
 * itself are listed; an inherited one belongs to the base's own metaobject.
 */
[[nodiscard]] inline std::string describe(QMetaObject const& meta_object)
{
  std::string out = std::format("class {}\n", meta_object.className());

  for(int i = meta_object.classInfoOffset(); i < meta_object.classInfoCount(); ++i)
  {
    const QMetaClassInfo info = meta_object.classInfo(i);
    out += std::format("  classinfo   {} = {}\n", info.name(), info.value());
  }

  for(int i = meta_object.methodOffset(); i < meta_object.methodCount(); ++i)
  {
    const QMetaMethod method = meta_object.method(i);
    out += std::format("  {:<11} {}\n",
                       detail::method_kind_name(method.methodType()),
                       method.methodSignature().constData());
  }

  for(int i = meta_object.propertyOffset(); i < meta_object.propertyCount(); ++i)
  {
    const QMetaProperty property = meta_object.property(i);
    out += std::format("  property    {} : {}\n", property.name(), property.typeName());
  }

  for(int i = meta_object.enumeratorOffset(); i < meta_object.enumeratorCount(); ++i)
  {
    const QMetaEnum enumerator = meta_object.enumerator(i);
    out += std::format("  enum        {}\n", enumerator.name());
    for(int k = 0; k < enumerator.keyCount(); ++k)
    {
      out += std::format("    {} = {}\n", enumerator.key(k), enumerator.value(k));
    }
  }
  return out;
}

[[nodiscard]] inline std::string describe(QObject const& object)
{
  return describe(*object.metaObject());
}

template <typename T> [[nodiscard]] std::string describe()
{
  return describe(T::staticMetaObject);
}

/** @brief prints what `describe` renders, to stdout */
inline void dump(QMetaObject const& meta_object)
{
  std::print("{}", describe(meta_object));
}

inline void dump(QObject const& object)
{
  std::print("{}", describe(object));
}

template <typename T> void dump()
{
  std::print("{}", describe<T>());
}
}
