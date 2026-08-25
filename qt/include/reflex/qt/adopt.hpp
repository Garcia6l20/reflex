#pragma once

/** @file
 *
 * Publishes a struct to Qt as a gadget without deriving `qt::gadget<T>`, for a
 * type whose definition cannot be changed: a third-party one, or one a
 * generator writes.
 *
 * ```cpp
 * struct point { int x = 0; int y = 0; };
 *
 * consteval { reflex::qt::make_gadget(^^point); }
 * ```
 *
 * The mark is a specialization of a template declared and never defined,
 * completed by `define_aggregate`, and read back through @ref adopted. Two
 * rules come with that, and neither is diagnosed:
 *
 * - the consteval block sits at global scope, because a specialization has to
 *   be declared in a namespace enclosing the template it specializes, and the
 *   marker template is one of reflex.qt's own
 * - the block precedes every query of `adopted<T>` for the same type, because
 *   the variable template's value is cached from its first instantiation, so a
 *   later mark is silently lost
 *
 * An adopted struct publishes what a gadget publishes, minus what an
 * annotation carries: a property still needs `[[= qt::prop{}]]`, so a type
 * whose definition is genuinely out of reach is adopted for its metatype and
 * its class name rather than for a property table.
 *
 * `qt::object` has no counterpart here. `metaObject()`, `qt_metacast` and
 * `qt_metacall` are virtual overrides and a real `QObject` base, and nothing
 * outside a class body supplies those.
 */

#include <reflex/const_check.hpp>
#include <reflex/meta.hpp>
#include <reflex/qt/detail/gadget_impl.hpp>
#include <reflex/qt/detail/version.hpp>

#include <QtCore/qmetaobject.h>
#include <QtCore/qmetatype.h>

#include <string>

namespace reflex::qt
{
namespace detail
{
template <typename T> struct adopted_marker;

consteval void mark_adopted(meta::info T)
{
  define_aggregate(substitute(^^adopted_marker, {T}), {});
}
}

/** @brief whether `make_gadget` has published @p T to Qt */
template <typename T>
constexpr bool adopted = meta::is_complete_type(substitute(^^detail::adopted_marker, {^^T}));

/** @brief whether `make_gadget` has published the type @p T to Qt */
consteval bool is_adopted(meta::info T)
{
  return meta::is_type(T) and not meta::is_type_alias(T) and meta::is_class_type(T)
     and meta::is_complete_type(T) and extract<bool>(substitute(^^adopted, {T}));
}

/** @brief Publishes the class @p T to Qt as a gadget.
 *
 * @throws std::meta::exception when @p T is not a complete class type, or when
 *         it already derives `qt::gadget`, which publishes it already.
 */
consteval void make_gadget(meta::info T)
{
  REFLEX_META_CHECK(meta::is_type(T) and not meta::is_type_alias(T) and meta::is_class_type(T)
                        and meta::is_complete_type(T),
                    std::string{meta::display_string_of(T)}
                        + " is not a complete class type and cannot be adopted as a gadget",
                    T);
  REFLEX_META_CHECK(not meta::is_subclass_of(T, ^^qt::gadget, meta::access_context::unchecked()),
                    std::string{meta::display_string_of(T)}
                        + " already derives reflex::qt::gadget and is published to Qt already",
                    T);
  detail::mark_adopted(T);
}

/** @brief the `QMetaObject` of an adopted @p T, where a gadget carries its own */
template <typename T>
  requires adopted<T>
inline const QMetaObject adopted_meta_object = detail::gadget_impl<T>::metaObject();

/** @brief the `QMetaObject` of @p T, adopted or deriving a reflex.qt base */
template <typename T> const QMetaObject& meta_object_of()
{
  if constexpr(adopted<T>)
  {
    return adopted_meta_object<T>;
  }
  else
  {
    return T::staticMetaObject;
  }
}
}

QT_BEGIN_NAMESPACE
namespace QtPrivate
{
/** @brief points Qt at the metaobject an adopted type keeps outside itself
 *
 * `IsGadgetHelper` is deliberately left unclaimed for an adopted type. Claiming
 * it makes Qt's own `MetaObjectForType` partial specialization match, and that
 * one hardwires `&T::staticMetaObject`, which an adopted type does not have.
 * The two specializations are then ambiguous, since neither is more
 * specialized. Leaving the trait alone makes this one the sole candidate.
 */
template <typename T>
  requires reflex::qt::adopted<T>
struct MetaObjectForType<T, void>
{
  static constexpr const QMetaObject* value()
  {
    return &reflex::qt::adopted_meta_object<T>;
  }

  static constexpr const QMetaObject* metaObjectFunction(const QMetaTypeInterface*)
  {
    return value();
  }
};

/** @brief restores the `QMetaType::IsGadget` flag `IsGadgetHelper` would carry
 *
 * The flag is what a QML value type turns on: `QQmlTypePrivate` hands back no
 * metaobject without it. Its only source is `QMetaTypeForType<T>::flags()`,
 * guarded by the one trait an adopted type must leave unclaimed, so the
 * interface object is re-listed here with the flag set. Every field but
 * `flags` and `metaObjectFn` delegates to the same helper Qt's own body
 * delegates to.
 */
template <typename T>
  requires reflex::qt::adopted<T>
struct QMetaTypeInterfaceWrapper<T>
{
  static_assert(QMetaTypeInterface::CurrentRevision == 1,
                "QMetaTypeInterface gained a field; reflex.qt re-lists the aggregate for an "
                "adopted gadget and has to follow");
  static_assert(BuiltinMetaType<T>::value == 0,
                "an adopted gadget cannot also be one of Qt's built-in types");

  static constexpr bool IsConstMetaTypeInterface = false;
  using InterfaceType                            = NonConstMetaTypeInterface;

  static inline InterfaceType metaType = {
      .revision         = QMetaTypeInterface::CurrentRevision,
      .alignment        = alignof(T),
      .size             = sizeof(T),
      .flags            = QMetaTypeForType<T>::flags() | uint(QMetaType::IsGadget),
      .typeId           = {BuiltinMetaType<T>::value},
      .metaObjectFn     = MetaObjectForType<T>::metaObjectFunction,
      .name             = QMetaTypeForType<T>::getName(),
      .defaultCtr       = QMetaTypeForType<T>::getDefaultCtr(),
      .copyCtr          = QMetaTypeForType<T>::getCopyCtr(),
      .moveCtr          = QMetaTypeForType<T>::getMoveCtr(),
      .dtor             = QMetaTypeForType<T>::getDtor(),
      .equals           = QEqualityOperatorForType<T>::equals,
      .lessThan         = QLessThanOperatorForType<T>::lessThan,
      .debugStream      = QDebugStreamOperatorForType<T>::debugStream,
      .dataStreamOut    = QDataStreamOperatorForType<T>::dataStreamOut,
      .dataStreamIn     = QDataStreamOperatorForType<T>::dataStreamIn,
      .legacyRegisterOp = QMetaTypeForType<T>::getLegacyRegister(),
  };
};
}
QT_END_NAMESPACE
