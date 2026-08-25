#pragma once

#include <reflex/qt/detail/gadget_impl.hpp>

#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>

#include <cstring>

namespace reflex::qt::detail
{
/** @brief the index arithmetic `qt_metacall` does, shared by every class
 *
 * A class's own dispatcher answers the calls that land inside its tables and
 * the id counts down towards its base, exactly as the body moc writes. None of
 * that depends on the class, so it is written once here and reached with the
 * two table sizes and the address the `QMetaObject` already holds.
 */
[[gnu::noinline]] inline int dispatch_metacall(
    QObject*          self,
    QMetaObject::Call c,
    int               id,
    void**            a,
    int               method_count,
    int               property_count,
    void (*dispatch)(QObject*, QMetaObject::Call, int, void**))
{
  if(c == QMetaObject::InvokeMetaMethod)
  {
    if(id < method_count)
    {
      dispatch(self, c, id, a);
    }
    id -= method_count;
  }
  if(c == QMetaObject::RegisterMethodArgumentMetaType)
  {
    if(id < method_count)
    {
      *reinterpret_cast<QMetaType*>(a[0]) = QMetaType();
    }
    id -= method_count;
  }
  switch(c)
  {
    case QMetaObject::ReadProperty:
    case QMetaObject::WriteProperty:
    case QMetaObject::ResetProperty:
    case QMetaObject::BindableProperty:
    case QMetaObject::RegisterPropertyMetaType:
      dispatch(self, c, id, a);
      id -= property_count;
      break;
    default:
      break;
  }
  return id;
}

/** @brief the `qt_metacall` / `qt_metacast` bodies moc writes into a `Q_OBJECT` class
 *
 * Both walk the local method and property tables, then hand what is left over
 * to @p ParentT so that a caller's index keeps counting down the base chain.
 */
template <typename Super, typename ParentT> struct object_impl
{
  using strings = typename gadget_impl<Super>::strings;

  static constexpr int method_count   = int(strings::method_count);
  static constexpr int property_count = int(strings::property_count);

  static int metacall(Super* self, QMetaObject::Call c, int id, void** a)
  {
    return dispatch_metacall(
        self, c, id, a, method_count, property_count, &gadget_impl<Super>::qt_static_metacall);
  }

  static void* metacast(Super* self, const char* clname)
  {
    if(not clname)
    {
      return nullptr;
    }
    if(std::strcmp(clname, gadget_impl<Super>::class_name()) == 0)
    {
      return static_cast<void*>(self);
    }
    return self->ParentT::qt_metacast(clname);
  }
};
}
