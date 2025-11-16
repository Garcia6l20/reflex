#pragma once

#include <QtCore/qmetatype.h>
#include <QtCore/qtmochelpers.h>
#include <QtCore/qxptype_traits.h>

#include <reflex/qt/detail/annotations.hpp>
#include <reflex/qt/detail/meta_strings.hpp>
#include <reflex/utility.hpp>

#ifdef __REFLEX_QT_ENABLE_TYPE_REGISTRY
#include <reflex/regitry.hpp>
#endif

namespace reflex::qt
{
namespace detail
{
#ifdef __REFLEX_QT_ENABLE_TYPE_REGISTRY
struct objects_scope
{
};
using object_reg = meta::registry<objects_scope>;
#endif

template <typename T> QMetaObject metaObjectOf();

} // namespace detail

template <typename Super> class gadget
{
public:
  static inline const QMetaObject staticMetaObject = detail::metaObjectOf<Super>();

  static void qt_static_metacall(QObject* _o, QMetaObject::Call _c, int _id, void** _a)
  {
    return detail::gadget_impl<Super>::qt_static_metacall(_o, _c, _id, _a);
  }

  // NOTE: QtGadgetHelper/qt_check_for_QGADGET_macro are required for gadget registration
  //       This is the way Qt checks for Q_GADGET macro, hacked !
  void         qt_check_for_QGADGET_macro();
  typedef void QtGadgetHelper;

private:
  friend class detail::gadget_impl<Super>;
  template <typename, typename> friend class object_impl;

  struct tag
  {
  };

#ifdef __REFLEX_QT_ENABLE_TYPE_REGISTRY
  consteval
  {
    detail::object_reg::add(^^Super);
  };
#endif

protected:
  static constexpr detail::invocable invocable;

  using prop = detail::property;

  template <meta::info of> static constexpr detail::listener_of<of> listener_of;
  template <meta::info of> static constexpr detail::getter_of<of>   getter_of;
  template <meta::info of> static constexpr detail::setter_of<of>   setter_of;

  static consteval auto parameters_types_of(meta::info R)
  {
    if(is_function(R))
    {
      return parameters_of(R) | std::views::transform(meta::type_of);
    }
    else
    {
      // assume signal
      auto type    = type_of(R);
      auto call_fn = meta::member_named(type, "fn", meta::access_context::unchecked());
      return parameters_of(call_fn) //
           | std::views::transform(meta::type_of);
    }
  }

  template <meta::info R> static consteval auto parameters_types_of()
  {
    return parameters_types_of(R);
  }

  template <meta::info Property> auto property(this auto& self)
  {
    static constexpr auto ms = detail::meta_strings<tag, Super>{};

    using std::ranges::contains;
    static constexpr auto props = ms.properties;
    if constexpr(contains(props, Property))
    {
      static constexpr auto getter =
          meta::first_member_function_annotated_with(^^Super, //
                                                     ^^detail::getter_of<Property>,
                                                     meta::access_context::unchecked());
      if constexpr(getter != meta::null)
      {
        return self.[:getter:]();
      }
      else
      {
        return self.[:Property:];
      }
    }
    else
    {
      static_assert(always_false<Super>, "no such property");
    }
  }

  template <meta::info Property> void notifyPropertyChanged(this auto& self)
  {
    static constexpr auto ms = detail::meta_strings<tag, Super>{};
    if constexpr(ms.is_object)
    {
      static constexpr auto props = ms.properties;
      static constexpr auto it    = std::ranges::find(props, Property);
      if constexpr(it != end(props))
      {
        static constexpr auto relative_offset         = std::distance(begin(props), it);
        static constexpr auto notifier_signals_offset = ms.signal_count;
        QMetaObject::activate<void>(&self,
                                    &staticMetaObject,
                                    notifier_signals_offset + relative_offset,
                                    nullptr);
      }
      else
      {
        static_assert(always_false<Super>, "no such property");
      }
    }
    else
    {
      static_assert(always_false<Super>, "gadget cannot notify for property changes");
    }
  }

public:
  template <constant_string name> auto property(this auto& self)
  {
    using std::ranges::contains;
    static constexpr auto prop =
        meta::member_named(^^Super, *name, meta::access_context::unchecked());
    static_assert(prop != meta::null, "no such property");
    return self.template property<prop>();
  }

  template <meta::info Property, typename T> void setProperty(this auto& self, T&& value)
  {
    static constexpr auto ms = detail::meta_strings<tag, Super>{};

    using std::ranges::find;
    static constexpr auto props = ms.properties;
    static constexpr auto it    = find(props, Property);
    if constexpr(it != end(props))
    {
      static constexpr auto setter =
          meta::first_member_function_annotated_with(^^Super, //
                                                     ^^detail::setter_of<Property>,
                                                     meta::access_context::unchecked());
      if constexpr(setter != meta::null)
      {
        self.[:setter:](value);
      }
      else
      {
        auto& val = self.[:Property:];
        if(value == val)
        {
          return; // no write need
        }
        val = std::forward<T>(value);
      }

      static constexpr auto listener =
          meta::first_member_function_annotated_with(^^Super, //
                                                     ^^detail::listener_of<Property>,
                                                     meta::access_context::unchecked());
      if constexpr(listener != meta::null)
      {
        self.[:listener:]();
      }

      if constexpr(ms.is_object)
      {
        static constexpr auto relative_offset         = std::distance(begin(props), it);
        static constexpr auto notifier_signals_offset = ms.signal_count;
        QMetaObject::activate<void>(&self,
                                    &staticMetaObject,
                                    notifier_signals_offset + relative_offset,
                                    nullptr);
      }
    }
    else
    {
      static_assert(always_false<Super>, "no such property");
    }
  }

  template <constant_string name, typename T> void setProperty(this auto& self, T&& value)
  {
    using std::ranges::contains;
    static constexpr auto prop =
        meta::member_named(^^Super, *name, meta::access_context::unchecked());
    static_assert(prop != meta::null, "no such property");
    self.template setProperty<prop>(std::forward<T>(value));
  }

  template <constant prop> void propertyChanged()
  {
    std::unreachable(); // NOTE: actually unused, just to allow QObject::connect to match the
                        // auto-generated signal
  }
};

#define REFLEX_QT_GADGET_IMPL(__type__) template class reflex::qt::gadget<__type__>;
} // namespace reflex::qt

QT_BEGIN_NAMESPACE
template <typename Super>
  requires(reflex::meta::is_subclass_of(^^Super, ^^reflex::qt::gadget))
struct QMetaTypeId<Super>
{
  enum
  {
    Defined = 1
  };
  static_assert(QtPrivate::checkTypeIsSuitableForMetaType<Super>());
  static int qt_metatype_id()
  {
    Q_CONSTINIT static QBasicAtomicInt metatype_id = Q_BASIC_ATOMIC_INITIALIZER(0);
    if(const int id = metatype_id.loadAcquire())
      return id;
    constexpr auto arr  = QtPrivate::typenameHelper<Super>();
    auto           name = arr.data();
    if(QByteArrayView(name) == display_string_of(^^Super).data())
    {
      const int id = qRegisterNormalizedMetaType<Super>(name);
      metatype_id.storeRelease(id);
      return id;
    }
    const int newId = qRegisterMetaType<Super>(display_string_of(^^Super).data());
    metatype_id.storeRelease(newId);
    return newId;
  }
};
QT_END_NAMESPACE
