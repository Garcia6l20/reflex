#pragma once

#include <reflex/qt.hpp>
#include <reflex/qt/moc/export.hpp>

#include <QtCore/qstring.h>

/** @file
 *
 * The reflex.qt side of the metatypes cross-check. `twin` matches `mirror` and
 * `twin_qml` matches `mirror_qml`, member for member and in the same order, at
 * global scope so that the enumeration's qualified name differs from moc's by
 * the class name alone.
 */
namespace mocqt = reflex::qt;

struct [[= mocqt::qml{}]] [[= mocqt::classinfo{"author", "reflex"}]] twin
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

  [[= mocqt::prop{.constant = true}]] int fixed = 7;

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

/** @brief one `qml` annotation standing for the six macros `mirror_qml` carries */
struct [[= mocqt::qml{.name        = "Gauge",
                      .singleton   = true,
                      .uncreatable = "ask the factory",
                      .added_in    = {1, 2},
                      .removed_in  = {2, 0}}]] twin_qml : mocqt::object<twin_qml>
{
  friend mocqt::access<twin_qml>;

  [[= mocqt::prop{}]] int level = 0;
};

/** @brief the gadget half of the pair, matching `mirror_gadget` */
struct [[= mocqt::qml{.name = "span"}]] twin_gadget : mocqt::gadget<twin_gadget>
{
  [[= mocqt::prop{}]] int length = 0;

  [[= mocqt::invocable]] int doubled() const
  {
    return length * 2;
  }
};
