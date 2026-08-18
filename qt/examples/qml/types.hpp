#pragma once

#include <reflex/qt.hpp>

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

namespace qt = reflex::qt;

/** @brief a counter published to QML, with no Q_OBJECT and no moc */
class [[= qt::classinfo{"QML.Element", "auto"}]] Counter : public qt::object<Counter>
{
  friend reflex::qt::access<Counter>;

public:
  using qt::object<Counter>::object;

  /** @brief how many times @ref bump has been called */
  [[= qt::prop{}]] int value{0};

  /** @brief a greeting naming the current @ref value */
  [[= qt::invocable]] QString caption() const
  {
    return QStringLiteral("counted to %1").arg(value);
  }

  /** @brief add @p by to @ref value */
  [[= qt::invocable]] void bump(int by) { setProperty<"value">(value + by); }
};
