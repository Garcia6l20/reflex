#pragma once

#include <reflex/qt.hpp>
#include <reflex/qt/moc/export.hpp>

#include <QtCore/qnamespace.h>
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

  enum class Level : unsigned char
  {
    Low,
    High
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

  [[= mocqt::prop{}]] Qt::Alignment align = Qt::AlignLeft;

  signal<int> bumped{this};

private:
  /** @brief a signal declared private, which moc has no spelling for
   *
   * `Q_SIGNALS` is a public access specifier, so `mirror` declares `nudged`
   * public. The pair agrees only while reflex stamps a signal `AccessPublic`
   * whatever section it sits in, which is what the QML engine's property cache
   * reads.
   */
  signal<int> nudged{this};

public:
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

  [[= mocqt::prop{}]] unsigned int span_uint = 0;

  [[= mocqt::prop{}]] short span_short = 0;

  [[= mocqt::prop{}]] unsigned short span_ushort = 0;

  [[= mocqt::prop{}]] unsigned char span_uchar = 0;

  [[= mocqt::prop{}]] signed char span_schar = 0;

  [[= mocqt::prop{}]] long span_long = 0;

  [[= mocqt::prop{}]] unsigned long span_ulong = 0;

  [[= mocqt::prop{}]] long long span_llong = 0;

  [[= mocqt::prop{}]] unsigned long long span_ullong = 0;

  [[= mocqt::invocable]] int doubled() const
  {
    return length * 2;
  }

  [[= mocqt::slot]] void stretch(int by = 1)
  {
    length += by;
  }
};

/** @brief the base of the inheritance chain, matching `mirror_base` */
struct twin_base : mocqt::object<twin_base>
{
  [[= mocqt::prop{}]] int level = 0;

  [[= mocqt::slot]] void climb()
  {
    ++level;
  }
};

/** @brief a class deriving another reflex.qt class, matching `mirror_derived` */
struct twin_derived : mocqt::object<twin_derived, twin_base>
{
  [[= mocqt::prop{}]] int depth = 0;

  [[= mocqt::slot]] void dive()
  {
    ++depth;
  }
};

/** @brief a class declaring its own flags and a protected slot, matching `mirror_flags` */
struct twin_flags : mocqt::object<twin_flags>
{
  friend mocqt::access<twin_flags>;

  enum Option
  {
    NoOption = 0x0,
    First    = 0x1,
    Second   = 0x2
  };

  using Options = QFlags<Option>;

  [[= mocqt::prop{}]] Options options;

protected:
  [[= mocqt::slot]] void guarded()
  {
    options = {};
  }
};

/** @brief accessors found by convention rather than by annotation, matching `mirror_styled` */
struct[[= mocqt::naming::qt_style]] twin_styled : mocqt::object<twin_styled>
{
  friend mocqt::access<twin_styled>;

  [[= mocqt::prop{}]] int weight = 0;

  int getWeight() const
  {
    return weight;
  }

  void setWeight(int value)
  {
    weight = value;
  }

  void onWeightChanged()
  {
    ++notifications;
  }

  int notifications = 0;
};
