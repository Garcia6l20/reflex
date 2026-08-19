#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/access.hpp>
#include <reflex/qt/detail/annotations.hpp>
#include <reflex/qt/detail/gadget_impl.hpp>

#include <QtCore/qmetaobject.h>
#include <QtCore/qmetatype.h>

#include <type_traits>
#include <utility>

namespace reflex::qt
{
/** @brief CRTP base publishing @p Super to Qt as a `Q_GADGET`
 *
 * ```cpp
 * namespace qt = reflex::qt;
 *
 * struct point : qt::gadget<point>
 * {
 *   [[= qt::prop{}]] int x = 0;
 *   [[= qt::invocable]] int twice() const { return 2 * x; }
 * };
 * ```
 *
 * No `Q_GADGET` macro is written and moc never runs: `staticMetaObject` is
 * built from reflection over @p Super, and the `QtPrivate::IsGadgetHelper`
 * specializations below tell Qt to accept @p Super as a gadget.
 */
template <typename Super> class gadget
{
public:
  static inline const QMetaObject staticMetaObject = detail::gadget_impl<Super>::metaObject();

  template <meta::info Property> auto property(this auto& self)
  {
    [[maybe_unused]] static constexpr bool checked = detail::check_readable(^^Super, Property);

    static constexpr auto reader = detail::accessor_for<^^getter_t>(^^Super, Property);
    if constexpr(reader != meta::null)
    {
      return access<Super>::template call<reader>(self);
    }
    else
    {
      return access<Super>::template member<Property>(self);
    }
  }

  /** @brief Reads the property named @p name, including one a base declares.
   *
   * The lookup searches @p Super and its bases, so a property inherited from
   * another reflex class is reachable from the derived type. A property
   * declared in @p Super shadows one of the same name in a base.
   */
  template <constant_string name> auto property(this auto& self)
  {
    static constexpr auto p = detail::required_member_named(^^Super, *name);
    using owner             = [:meta::parent_of(p):];
    return static_cast<owner&>(self).template property<p>();
  }

  template <meta::info Property, typename T> void setProperty(this auto& self, T&& value)
  {
    [[maybe_unused]] static constexpr bool checked = detail::check_writable(^^Super, Property);

    static constexpr auto writer  = detail::accessor_for<^^setter_t>(^^Super, Property);
    static constexpr auto handler = detail::accessor_for<^^listener_t>(^^Super, Property);

    if constexpr(writer != meta::null)
    {
      access<Super>::template call<writer>(self, std::forward<T>(value));
    }
    else
    {
      auto& target = access<Super>::template member<Property>(self);
      if constexpr(requires { target == value; })
      {
        if(target == value)
        {
          return;
        }
      }
      target = std::forward<T>(value);
    }
    if constexpr(handler != meta::null)
    {
      access<Super>::template call<handler>(self);
    }
    if constexpr(detail::gadget_impl<Super>::strings::is_object
                 and detail::property_spec_of(Property).notifying())
    {
      self.template propertyChanged<constant_string{identifier_of(Property)}>();
    }
  }

  /** @brief Writes @p value to the property named @p name, notifying once.
   *
   * The lookup searches @p Super and its bases. An inherited property is
   * written and notified through the class that declares it, so its notify
   * signal carries that class's own method index. A write of the value the
   * property already holds notifies nothing.
   */
  template <constant_string name, typename T> void setProperty(this auto& self, T&& value)
  {
    static constexpr auto p = detail::required_member_named(^^Super, *name);
    using owner             = [:meta::parent_of(p):];
    static_cast<owner&>(self).template setProperty<p>(std::forward<T>(value));
  }
};
}

QT_BEGIN_NAMESPACE
namespace QtPrivate
{
template <typename Super>
  requires(reflex::meta::is_complete_type(^^Super)
           and reflex::meta::is_subclass_of(^^Super,
                                            ^^reflex::qt::gadget,
                                            reflex::meta::access_context::unchecked())
           and not IsPointerToTypeDerivedFromQObject<Super*>::Value)
struct IsGadgetHelper<Super, void>
{
  enum
  {
    IsRealGadget          = true,
    IsGadgetOrDerivedFrom = true
  };
};

template <typename Super>
  requires(reflex::meta::is_complete_type(^^Super)
           and reflex::meta::is_subclass_of(^^Super,
                                            ^^reflex::qt::gadget,
                                            reflex::meta::access_context::unchecked())
           and not IsPointerToTypeDerivedFromQObject<Super*>::Value)
struct IsPointerToGadgetHelper<Super*, void>
{
  using BaseType = Super;

  enum
  {
    IsRealGadget          = true,
    IsGadgetOrDerivedFrom = true
  };
};
}

template <typename Super>
  requires(reflex::meta::is_complete_type(^^Super)
           and reflex::meta::is_subclass_of(^^Super,
                                            ^^reflex::qt::gadget,
                                            reflex::meta::access_context::unchecked()))
struct QMetaTypeId<Super>
{
  enum
  {
    Defined = std::is_default_constructible_v<Super>
  };

  static int qt_metatype_id()
  {
    Q_CONSTINIT static QBasicAtomicInt metatype_id = Q_BASIC_ATOMIC_INITIALIZER(0);
    if(const int id = metatype_id.loadAcquire())
    {
      return id;
    }
    const auto name  = reflex::meta::display_string_of(^^Super);
    const int  newId = qRegisterNormalizedMetaType<Super>(
        QByteArray(name.data(), qsizetype(name.size())));
    metatype_id.storeRelease(newId);
    return newId;
  }
};
QT_END_NAMESPACE
