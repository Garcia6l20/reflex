#pragma once

#include <reflex/qt/qml.hpp>

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

namespace qt = reflex::qt;

namespace clock_example
{
/** @brief a ticking clock published to QML, with no `Q_OBJECT` and no moc
 *
 * `qt::qml{}` publishes it under its own name, so QML instantiates it as
 * `Clock`. Its timer, its properties and their notify signals all come from
 * reflection over this declaration.
 */
class [[= qt::qml{}]] Clock : public qt::object<Clock>
{
  friend qt::access<Clock>;

public:
  using qt::object<Clock>::object;

  /** @brief the current reading, empty until the first tick */
  [[= qt::prop{}]] QString label{};

  /** @brief how many times the clock has ticked */
  [[= qt::prop{}]] int ticks{0};

  /** @brief emitted once the clock has run its five ticks */
  signal<> finished{this};

  /** @brief Starts ticking every @p period_ms milliseconds. */
  [[= qt::invocable]] void start(int period_ms)
  {
    startTimer<^^tick>(period_ms);
  }

  /** @brief Stops the clock. */
  [[= qt::invocable]] void stop()
  {
    killTimer<^^tick>();
  }

  /** @brief Records that QML re-evaluated a binding on @ref label. */
  [[= qt::invocable]] void observe()
  {
    ++observed_;
  }

  /** @brief how many times @ref observe has been called */
  [[= qt::invocable]] int observed() const
  {
    return observed_;
  }

private:
  void tick()
  {
    setProperty<^^ticks>(ticks + 1);
    setProperty<^^label>(QStringLiteral("tick %1").arg(ticks));
    if(ticks == run_length)
    {
      stop();
      finished();
    }
  }

  static constexpr int run_length = 5;

  qt::timer<^^tick> ticker{};
  int               observed_ = 0;
};
} // namespace clock_example
