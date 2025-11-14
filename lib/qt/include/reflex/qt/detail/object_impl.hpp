#pragma once

#include <reflex/qt/object.hpp>

#include <reflex/qt/detail/gadget_impl.hpp>
#include <reflex/qt/detail/metatype.hpp>

#include <QObject>
#include <QTimerEvent>
#include <QtCore/qmetatype.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qxptype_traits.h>

#undef signals
#undef slots

namespace reflex::qt
{

namespace detail
{
template <typename Super, typename Parent> struct object_impl
{
  using object_type = object<Super, Parent>;
  using tag         = object_type::tag;

  static inline int qt_metacall(object_type* obj, QMetaObject::Call _c, int _id, void** _a)
  {
    static constexpr auto ms = detail::meta_strings<tag, Super>{};

    static constexpr auto signal_slot_count = [&]
    {
      size_t count = 0;
      count += ms.signal_count;
      count += ms.properties.size(); /* no args for property notifiers */
      count += ms.slot_count;
      count += ms.invocable_count;
      return count;
    }();

    _id = static_cast<Parent*>(obj)->Parent::qt_metacall(_c, _id, _a);
    if(_id < 0)
      return _id;

    if(_c == QMetaObject::InvokeMetaMethod)
    {
      if(_id <= signal_slot_count)
        Super::qt_static_metacall(obj, _c, _id, _a);
      _id -= signal_slot_count;
    }
    if(_c == QMetaObject::RegisterMethodArgumentMetaType)
    {
      if(_id <= signal_slot_count)
        *reinterpret_cast<QMetaType*>(_a[0]) = QMetaType();
      _id -= signal_slot_count;
    }
    if(_c
       == QMetaObject::ReadProperty
       || _c
       == QMetaObject::WriteProperty
       || _c
       == QMetaObject::ResetProperty
       || _c
       == QMetaObject::BindableProperty
       || _c
       == QMetaObject::RegisterPropertyMetaType)
    {
      Super::qt_static_metacall(obj, _c, _id, _a);
      _id -= 1;
    }
    return _id;
  }
};
} // namespace detail

template <typename Super, typename Parent> void* object<Super, Parent>::qt_metacast(const char* _clname)
{
  if(!_clname)
    return nullptr;
  if(!strcmp(_clname, detail::gadget_impl<Super>::template qt_staticMetaObjectStaticContent<tag>.strings))
    return static_cast<void*>(this);
  return QObject::qt_metacast(_clname);
}
template <typename Super, typename Parent>
int object<Super, Parent>::qt_metacall(QMetaObject::Call _c, int _id, void** _a)
{
  return detail::object_impl<Super, Parent>::qt_metacall(this, _c, _id, _a);
}

} // namespace reflex::qt

#define REFLEX_QT_OBJECT_IMPL(__type__)                                \
  template <typename T> QMetaObject reflex::qt::detail::metaObjectOf() \
  {                                                                    \
    static auto mo = detail::gadget_impl<T>::metaObject();             \
    return mo;                                                         \
  }
