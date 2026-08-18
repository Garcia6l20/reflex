#pragma once

#include <reflex/qt.hpp>
#include <reflex/qt/moc/export.hpp>

#include <QtCore/qstring.h>

/** @file
 *
 * The reflex.qt side of the metatypes cross-check. `twin` matches `mirror` in
 * `moc-mirror.hpp` member for member, in the same order, at global scope so that
 * the enumeration's qualified name differs from moc's by the class name alone.
 */
namespace mocqt = reflex::qt;

struct [[= mocqt::classinfo{"QML.Element", "auto"}]] [[= mocqt::classinfo{"author", "reflex"}]] twin
    : mocqt::object<twin>
{
  friend mocqt::access<twin>;

  enum Mode
  {
    Fast,
    Slow
  };

private:
  [[= mocqt::prop{}]] int count = 0;

public:
  [[= mocqt::prop{}]] int extra = 0;

private:
  [[= mocqt::prop{.constant = true}]] QString title;

public:
  [[= mocqt::prop{.final = true, .required = true}]] Mode mode = Fast;

  signal<int> bumped{this};

  [[= mocqt::getter<^^count>]] int getCount() const
  {
    return count;
  }

  [[= mocqt::setter<^^count>]] void setCount(int value)
  {
    count = value;
  }

  [[= mocqt::getter<^^title>]] QString getTitle() const
  {
    return title;
  }

  [[= mocqt::invocable]] QString caption(int width) const
  {
    return QString::number(width);
  }

  [[= mocqt::invocable]] void reset()
  {
    setProperty<^^count>(0);
  }

  [[= mocqt::slot]] void increment(int by)
  {
    setProperty<^^count>(count + by);
  }

private:
  [[= mocqt::slot]] void hidden()
  {
  }
};
