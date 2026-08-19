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
} // namespace engine_test
