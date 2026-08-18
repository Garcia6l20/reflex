#pragma once

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
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
 * built from reflection over @p Super, and `QtGadgetHelper` is what Qt looks
 * for to accept the class as a gadget.
 */
template <typename Super> class gadget
{
public:
  static inline const QMetaObject staticMetaObject = detail::gadget_impl<Super>::metaObject();

  void         qt_check_for_QGADGET_macro();
  typedef void QtGadgetHelper;

  template <meta::info Property> auto property(this auto& self)
  {
    static_assert(std::ranges::contains(detail::gadget_impl<Super>::strings::properties, Property),
                  "no such property");
    return self.[:Property:];
  }

  template <constant_string name> auto property(this auto& self)
  {
    static constexpr auto p = meta::member_named(^^Super, *name, meta::access_context::unchecked());
    static_assert(p != meta::null, "no such property");
    return self.template property<p>();
  }

  template <meta::info Property, typename T> void setProperty(this auto& self, T&& value)
  {
    static_assert(std::ranges::contains(detail::gadget_impl<Super>::strings::properties, Property),
                  "no such property");
    self.[:Property:] = std::forward<T>(value);
  }

  template <constant_string name, typename T> void setProperty(this auto& self, T&& value)
  {
    static constexpr auto p = meta::member_named(^^Super, *name, meta::access_context::unchecked());
    static_assert(p != meta::null, "no such property");
    self.template setProperty<p>(std::forward<T>(value));
  }

protected:
  static constexpr detail::invocable invocable{};

  using prop = detail::property;
};
}

QT_BEGIN_NAMESPACE
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
