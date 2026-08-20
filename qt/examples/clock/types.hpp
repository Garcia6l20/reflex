#pragma once

#include <reflex/qt/qml.hpp>

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

namespace qt = reflex::qt;

namespace clock_example
{
/** @brief the run a clock ticks through, as a QML value type
 *
 * A gadget carries no `QObject`, so QML copies it by value. The lowercase name
 * is what `QML_VALUE_TYPE` requires of one.
 */
struct[[= qt::qml{.name = "span"}]] span : qt::gadget<span>
{
  span() = default;

  span(int lo, int hi) : low{lo}, high{hi}
  {}

  [[= qt::prop{}]] int low  = 0;
  [[= qt::prop{}]] int high = 0;

  /** @brief how many ticks the run holds */
  [[= qt::invocable]] int width() const
  {
    return high - low;
  }
};

/** @brief the one settings object the engine owns, reachable from QML by name */
class[[= qt::qml{.singleton = true}]] Settings : public qt::object<Settings>
{
  friend qt::access<Settings>;

public:
  using qt::object<Settings>::object;

  /** @brief what the window calls itself */
  [[= qt::prop{}]] QString title{QStringLiteral("reflex clock")};
};

/** @brief the shades a @ref Clock paints with, published for its enumeration alone */
class[[= qt::qml{.uncreatable = "Palette exists for its enumeration"}]] Palette
    : public qt::object<Palette>
{
  friend qt::access<Palette>;

public:
  using qt::object<Palette>::object;

  enum class Shade
  {
    light,
    mid,
    dark
  };
};

/** @brief a ticking clock published to QML, with no `Q_OBJECT` and no moc
 *
 * `qt::qml{}` publishes it under its own name, so QML instantiates it as
 * `Clock`. Its timer, its properties and their notify signals all come from
 * reflection over this declaration.
 */
class[[= qt::qml{.added_in = {1, 0}}]] Clock : public qt::object<Clock>
{
  friend qt::access<Clock>;

public:
  using qt::object<Clock>::object;

  /** @brief the current reading, empty until the first tick */
  [[= qt::prop{}]] QString label{};

  /** @brief how many times the clock has ticked */
  [[= qt::prop{}]] int ticks{0};

  /** @brief the run the clock stops at the end of */
  [[= qt::prop{}]] span run{0, 5};

  /** @brief the shade QML picks out of `Palette` */
  [[= qt::prop{}]] Palette::Shade shade = Palette::Shade::mid;

  /** @brief emitted once the clock has ticked through @ref run */
  signal<> finished{this};

  /** @brief carries @ref caption to whoever asked for a report */
  signal<QString> reported{this};

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

  /** @brief Rewinds to the start of @ref run. */
  [[= qt::invocable]] void rewind()
  {
    setProperty<^^ticks>(run.low);
    setProperty<^^label>(QString{});
  }

  /** @brief a copy of @p s grown by @p by at both ends */
  [[= qt::invocable]] span widen(span s, int by) const
  {
    return span{s.low - by, s.high + by};
  }

  /** @brief Emits @ref reported with the current @ref caption. */
  [[= qt::invocable]] void report()
  {
    reported(caption());
  }

  /** @brief Records @p note as the last thing QML had to say. */
  [[= qt::invocable]] void observe(QString const& note)
  {
    note_ = note;
    ++observed_;
  }

  /** @brief the current run and shade, as text */
  [[= qt::invocable]] QString caption() const
  {
    return QStringLiteral("span [%1, %2] width %3 shade %4")
        .arg(run.low)
        .arg(run.high)
        .arg(run.width())
        .arg(int(shade));
  }

  /** @brief how many times @ref observe has been called */
  [[= qt::invocable]] int observed() const
  {
    return observed_;
  }

  /** @brief the last note @ref observe was given */
  [[= qt::invocable]] QString note() const
  {
    return note_;
  }

private:
  void tick()
  {
    setProperty<^^ticks>(ticks + 1);
    setProperty<^^label>(QStringLiteral("tick %1").arg(ticks));
    if(ticks >= run.high)
    {
      stop();
      finished();
    }
  }

  qt::timer<^^tick> ticker{};
  QString           note_{};
  int               observed_ = 0;
};
} // namespace clock_example
