#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/access.hpp>
#include <reflex/qt/detail/annotations.hpp>
#include <reflex/qt/detail/gadget_impl.hpp>

#include <QtCore/qmetaobject.h>
#include <QtCore/qmetatype.h>

#include <algorithm>
#include <type_traits>
#include <utility>

namespace reflex::qt
{
/** @brief CRTP base publishing @p Super to Qt as a `Q_GADGET`
 *
 * ```cpp
 * struct point : reflex::qt::gadget<point>
 * {
 *   [[= prop{}]] int x = 0;
 *   [[= invocable]] int twice() const { return 2 * x; }
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
    static_assert(std::ranges::contains(detail::gadget_impl<Super>::strings::properties, Property),
                  "no such property");
    static_assert(detail::property_spec_of(Property).read, "the property is not readable");

    static constexpr auto reader = detail::accessor_for<^^detail::getter>(^^Super, Property);
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
    static constexpr auto p =
        meta::member_named(^^Super, *name, meta::access_context::unchecked(), true);
    static_assert(p != meta::null, "no such property");
    using owner = [:meta::parent_of(p):];
    return static_cast<owner&>(self).template property<p>();
  }

  template <meta::info Property, typename T> void setProperty(this auto& self, T&& value)
  {
    static_assert(std::ranges::contains(detail::gadget_impl<Super>::strings::properties, Property),
                  "no such property");
    static_assert(detail::property_spec_of(Property).writable(), "the property is not writable");

    static constexpr auto writer  = detail::accessor_for<^^detail::setter>(^^Super, Property);
    static constexpr auto handler = detail::accessor_for<^^detail::listener>(^^Super, Property);

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
    static constexpr auto p =
        meta::member_named(^^Super, *name, meta::access_context::unchecked(), true);
    static_assert(p != meta::null, "no such property");
    using owner = [:meta::parent_of(p):];
    static_cast<owner&>(self).template setProperty<p>(std::forward<T>(value));
  }

protected:
  static constexpr detail::invocable invocable{};

  using prop = detail::property;

  template <meta::info Property> static constexpr detail::getter<Property> getter{};
  template <meta::info Property> static constexpr detail::setter<Property> setter{};
  template <meta::info Property> static constexpr detail::listener<Property> listener{};
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
    const int newId = qRegisterNormalizedMetaType<Super>(Super::staticMetaObject.className());
    metatype_id.storeRelease(newId);
    return newId;
  }
};
QT_END_NAMESPACE
