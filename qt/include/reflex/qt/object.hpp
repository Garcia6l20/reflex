#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/detail/annotations.hpp>
#include <reflex/qt/detail/object_impl.hpp>
#include <reflex/qt/gadget.hpp>

#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>
#include <QtCore/qobjectdefs.h>

#include <ranges>
#include <utility>

namespace reflex::qt
{
/** @brief CRTP base publishing @p Super to Qt as a `Q_OBJECT` class
 *
 * ```cpp
 * struct counter : reflex::qt::object<counter>
 * {
 *   signal<int> changed{this};
 *
 *   [[= slot]] void bump() { setProperty<"value">(value + 1); }
 *
 *   [[= prop{}]] int value = 0;
 * };
 * ```
 *
 * No `Q_OBJECT` macro is written and moc never runs. `metaObject`,
 * `qt_metacast` and `qt_metacall` are overridden here, the `QMetaObject` comes
 * from `gadget<Super>`, and the `QtPrivate` specializations below make stock
 * Qt accept @p Super as a moc'ed class: `HasQ_OBJECT_Macro` so that `connect`
 * and `qobject_cast` admit it, and `FunctionPointer` so that new-style
 * `connect` accepts a `signal` data member where it expects a member function.
 *
 * Every property gets a `<name>Changed` notify signal, reachable from
 * `connect` as `&Super::propertyChanged<"name">`.
 */
template <typename Super, typename ParentT> class object : public ParentT, public gadget<Super>
{
  template <typename, typename...> friend class detail::signal_decl;

public:
  using parent_type = ParentT;

  using ParentT::ParentT;

  ~object() override = default;

  using gadget<Super>::staticMetaObject;

  const QMetaObject* metaObject() const override
  {
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
  }

  void* qt_metacast(const char* clname) override;
  int   qt_metacall(QMetaObject::Call c, int id, void** a) override;

  using ParentT::property;
  using gadget<Super>::property;

  using ParentT::setProperty;
  using gadget<Super>::setProperty;

  /** @brief emits the notify signal of the property named @p name */
  template <constant_string name> void propertyChanged()
  {
    using strings         = typename detail::gadget_impl<Super>::strings;
    constexpr auto index = strings::notifier_index_of(*name);
    static_assert(index < strings::method_count, "no such property");
    QMetaObject::activate(this, &staticMetaObject, int(index), nullptr);
  }

protected:
  template <typename... Args> using signal = detail::signal_decl<Super, Args...>;
  template <typename T> using with_default = detail::with_default<T>;

  static constexpr detail::slot slot{};

private:
  template <typename... Args, typename... CallArgs>
  void trigger(detail::signal_decl<Super, Args...>* sig, CallArgs const&... args);
};

template <typename Super, typename ParentT>
void* object<Super, ParentT>::qt_metacast(const char* clname)
{
  return detail::object_impl<Super, ParentT>::metacast(static_cast<Super*>(this), clname);
}

template <typename Super, typename ParentT>
int object<Super, ParentT>::qt_metacall(QMetaObject::Call c, int id, void** a)
{
  id = ParentT::qt_metacall(c, id, a);
  if(id < 0)
  {
    return id;
  }
  return detail::object_impl<Super, ParentT>::metacall(static_cast<Super*>(this), c, id, a);
}

template <typename Super, typename ParentT>
template <typename... Args, typename... CallArgs>
void object<Super, ParentT>::trigger(detail::signal_decl<Super, Args...>* sig,
                                     CallArgs const&... args)
{
  using strings = typename detail::gadget_impl<Super>::strings;

  auto& self = *static_cast<Super*>(this);
  template for(constexpr auto i : std::views::iota(0uz, strings::method_count))
  {
    if constexpr(strings::methods[i].kind == detail::method_kind::signal_member
                 and not strings::methods[i].cloned
                 and dealias(remove_const(type_of(strings::methods[i].member)))
                         == ^^detail::signal_decl<Super, Args...>)
    {
      if(&self.[:strings::methods[i].member:] == sig)
      {
        QMetaObject::activate<void>(this, &staticMetaObject, int(i), nullptr, args...);
        return;
      }
    }
  }
}
}

QT_BEGIN_NAMESPACE
namespace QtPrivate
{
template <typename Super>
  requires(reflex::meta::is_complete_type(^^Super)
           and reflex::meta::is_subclass_of(^^Super,
                                            ^^reflex::qt::object,
                                            reflex::meta::access_context::unchecked()))
struct HasQ_OBJECT_Macro<Super>
{
  enum
  {
    Value = 1
  };
};

template <typename ObjectT, typename... Args>
struct FunctionPointer<reflex::qt::detail::signal_decl<ObjectT, Args...> ObjectT::*>
{
  using Object     = ObjectT;
  using Arguments  = List<reflex::qt::detail::drop_default_t<Args>...>;
  using ReturnType = void;
  using Function   = ReturnType (reflex::qt::detail::signal_decl<ObjectT, Args...>::*)(
      reflex::qt::detail::drop_default_t<Args>...);

  enum
  {
    ArgumentCount             = sizeof...(Args),
    IsPointerToMemberFunction = true
  };

  template <typename SignalArgs, typename R> static void call(Function f, Object* o, void** arg)
  {
    FunctorCall<std::index_sequence_for<Args...>, SignalArgs, R, Function>::call(f, o, arg);
  }
};
}
QT_END_NAMESPACE
