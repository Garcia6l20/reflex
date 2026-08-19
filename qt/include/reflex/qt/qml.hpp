#pragma once

/** @file
 *
 * The `QQmlPrivate` traits `qmlRegisterTypesAndRevisions` reads, answered from
 * a `qt::qml` annotation.
 *
 * A singleton and an uncreatable type are decided by
 * `QQmlPrivate::QmlSingleton<T>` and `QQmlPrivate::QmlUncreatable<T>`, not by
 * the class infos in the metatypes document. Both read a nested enumeration and
 * a marker member function that `QML_SINGLETON` and `QML_UNCREATABLE` declare in
 * the class itself, and Qt compares the marker's owning class against `T`, so a
 * CRTP base cannot supply either. Specializing the two traits is what reflex.qt
 * has instead.
 *
 * The header the generated QML registration includes must reach this one, or the
 * class registers as an ordinary creatable type and its `QML.Singleton` class
 * info goes unread. It stays out of `reflex/qt.hpp`, because it pulls in QtQml.
 */

#include <reflex/meta.hpp>
#include <reflex/qt.hpp>
#include <reflex/qt/detail/annotations.hpp>

#include <QtQml/qqmlprivate.h>

namespace reflex::qt::detail
{
/** @brief a reflex.qt class published to QML by a `qt::qml` annotation */
template <typename Super>
concept qml_class =
    meta::is_complete_type(^^Super) and meta::is_class_type(^^Super)
    and (meta::is_subclass_of(^^Super, ^^qt::object, meta::access_context::unchecked())
         or meta::is_subclass_of(^^Super, ^^qt::gadget, meta::access_context::unchecked()))
    and meta::has_annotation(^^Super, ^^qt::qml);

/** @brief a `qml_class` whose annotation asks for a QML singleton */
template <typename Super>
concept qml_singleton_class = qml_class<Super> and qml_spec_of(^^Super).singleton;

/** @brief a `qml_class` whose annotation gives a reason QML may not create it */
template <typename Super>
concept qml_uncreatable_class =
    qml_class<Super> and not(*qml_spec_of(^^Super).uncreatable).empty();
}

QT_BEGIN_NAMESPACE
namespace QQmlPrivate
{
template <typename Super>
  requires reflex::qt::detail::qml_singleton_class<Super>
struct QmlSingleton<Super, void>
{
  static constexpr bool Value = true;
};

template <typename Super>
  requires reflex::qt::detail::qml_uncreatable_class<Super>
struct QmlUncreatable<Super, void>
{
  static constexpr bool Value = true;
};
}
QT_END_NAMESPACE
