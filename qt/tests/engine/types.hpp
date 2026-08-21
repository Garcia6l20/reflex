#pragma once

#include <reflex/qt/qml.hpp>

#include <QtCore/qobject.h>

namespace qt = reflex::qt;

/** @file
 *
 * The types the engine test publishes to QML. They exist to be built by a real
 * `QQmlApplicationEngine`, which is the only thing that reads a property cache
 * over a reflex class: `qmltyperegistrar` and `qmllint` accept shapes the
 * engine then refuses.
 */
namespace engine_test
{
/** @brief the one type `Main.qml` instantiates, published as `Probe` */
class [[= qt::qml{.name = "Probe"}]] probe : public qt::object<probe>
{
  friend qt::access<probe>;

public:
  using qt::object<probe>::object;

  /** @brief what @ref ping carries, written from QML */
  [[= qt::prop{}]] int level = 0;

  /** @brief Emits @ref pinged with @ref level plus one. */
  [[= qt::invocable]] void ping()
  {
    pinged(level + 1);
  }

private:
  signal<int> pinged{this};
};

/** @brief a class template, published to QML through @ref int_holder
 *
 * moc cannot parse an instantiation, so a `Q_OBJECT` template is a link error.
 * The metaobject here comes from reflection over `holder<T>` itself.
 */
template <typename T> class holder : public qt::object<holder<T>>
{
  using base = qt::object<holder<T>>;

public:
  template <typename... Args> using signal = typename base::template signal<Args...>;

  using base::base;

  [[= qt::prop{}]] T value{};

  /** @brief Emits @ref doubled with @ref value twice over. */
  [[= qt::invocable]] void twice()
  {
    doubled(value + value);
  }

  signal<T> doubled{this};
};

/** @brief `holder<int>` under a name QML can spell */
class [[= qt::qml{.name = "IntHolder"}]] int_holder : public qt::object<int_holder, holder<int>>
{
public:
  using qt::object<int_holder, holder<int>>::object;
};
} // namespace engine_test
