#pragma once

#include <reflex/qt/detail/gadget_impl.hpp>

#include <QtCore/qmetatype.h>
#include <QtCore/qobjectdefs.h>

#include <cstring>

namespace reflex::qt::detail
{
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
    if(c == QMetaObject::InvokeMetaMethod)
    {
      if(id < method_count)
      {
        gadget_impl<Super>::qt_static_metacall(self, c, id, a);
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
    if(c == QMetaObject::ReadProperty or c == QMetaObject::WriteProperty
       or c == QMetaObject::ResetProperty or c == QMetaObject::BindableProperty
       or c == QMetaObject::RegisterPropertyMetaType)
    {
      gadget_impl<Super>::qt_static_metacall(self, c, id, a);
      id -= property_count;
    }
    return id;
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
