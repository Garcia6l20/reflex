#pragma once

#include <reflex/qt/qml.hpp>

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

namespace qt = reflex::qt;

namespace sandbox
{
/** @brief a value type QML reads out of a property and hands back to an invocable
 *
 * A gadget carries no `QObject`, so QML copies it by value. The lowercase name
 * is what `QML_VALUE_TYPE` requires of one.
 */
struct [[= qt::qml{.name = "span"}]] span : qt::gadget<span>
{
  span() = default;

  span(int lo, int hi) : low{lo}, high{hi}
  {
  }

  [[= qt::prop{}]] int low  = 0;
  [[= qt::prop{}]] int high = 0;

  /** @brief how wide the span is */
  [[= qt::invocable]] int width() const
  {
    return high - low;
  }
};

/** @brief the one settings object the engine owns, reachable from QML by name */
class [[= qt::qml{.singleton = true}]] Settings : public qt::object<Settings>
{
  friend qt::access<Settings>;

public:
  using qt::object<Settings>::object;

  /** @brief what the sandbox calls itself */
  [[= qt::prop{}]] QString title{QStringLiteral("reflex sandbox")};
};

/** @brief the shades a @ref controller paints with, published for its enumeration alone */
class [[= qt::qml{.uncreatable = "Palette exists for its enumeration"}]] Palette
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

/** @brief the sandbox controller, published to QML as `Sandbox` */
class [[= qt::qml{.name = "Sandbox", .added_in = {1, 0}}]] controller
    : public qt::object<controller>
{
  friend qt::access<controller>;

public:
  using qt::object<controller>::object;

  /** @brief the span QML widens */
  [[= qt::prop{}]] span range{0, 10};

  /** @brief the shade QML picks out of `Palette` */
  [[= qt::prop{}]] Palette::Shade shade = Palette::Shade::mid;

  /** @brief carries @ref caption to whoever asked for a report */
  signal<QString> reported{this};

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

  /** @brief the current span and shade, as text */
  [[= qt::invocable]] QString caption() const
  {
    return QStringLiteral("span [%1, %2] width %3 shade %4")
        .arg(range.low)
        .arg(range.high)
        .arg(range.width())
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
  QString note_{};
  int     observed_ = 0;
};
} // namespace sandbox
