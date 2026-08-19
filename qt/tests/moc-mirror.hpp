#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtQmlIntegration/qqmlintegration.h>

/** @file
 *
 * The moc side of the metatypes cross-check: hand-written `Q_OBJECT` classes
 * whose members match `twin` and `twin_qml` in `moc-pair.hpp` one for one, in
 * the same order.
 * `moc --output-json` on this header and `write_metatypes` on `twin` must
 * produce the same document; `moc-cross-check.py` diffs them.
 *
 * moc never runs on it in the build. `test-moc-json.cpp` includes it so a change
 * to `twin` cannot leave the mirror behind as invalid C++.
 */
class mirror : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  Q_CLASSINFO("author", "reflex")

  Q_PROPERTY(int count READ getCount WRITE setCount NOTIFY countChanged)
  Q_PROPERTY(int extra MEMBER extra NOTIFY extraChanged)
  Q_PROPERTY(QString title READ getTitle CONSTANT)
  Q_PROPERTY(Mode mode MEMBER mode NOTIFY modeChanged FINAL REQUIRED)
  Q_PROPERTY(int fixed MEMBER fixed CONSTANT)
  Q_PROPERTY(Qt::Alignment align MEMBER align NOTIFY alignChanged)

public:
  enum Mode
  {
    Fast,
    Slow
  };
  Q_ENUM(Mode)

  enum class Level : unsigned char
  {
    Low,
    High
  };
  Q_ENUM(Level)

  int getCount() const
  {
    return count_;
  }

  void setCount(int value)
  {
    count_ = value;
  }

  QString getTitle() const
  {
    return {};
  }

  Q_INVOKABLE QString caption(int width) const
  {
    return QString::number(width);
  }

  Q_INVOKABLE void reset()
  {
    count_ = 0;
  }

  int  extra = 0;
  Mode mode  = Fast;
  int  fixed = 7;

  Qt::Alignment align = Qt::AlignLeft;

Q_SIGNALS:
  void bumped(int amount);
  void countChanged();
  void extraChanged();
  void modeChanged();
  void alignChanged();

public Q_SLOTS:
  void increment(int by)
  {
    count_ += by;
  }

private Q_SLOTS:
  void hidden()
  {
  }

private:
  int count_ = 0;
};

/** @brief the QML macros, as one class, mirroring `twin_qml`
 *
 * Every `QML_*` macro moc turns into a class info and nothing else, which is
 * what one `qt::qml{}` annotation stands for on the reflex side.
 */
class mirror_qml : public QObject
{
  Q_OBJECT
  QML_NAMED_ELEMENT(Gauge)
  QML_SINGLETON
  QML_UNCREATABLE("ask the factory")
  QML_ADDED_IN_VERSION(1, 2)
  QML_REMOVED_IN_VERSION(2, 0)

  Q_PROPERTY(int level MEMBER level NOTIFY levelChanged)

public:
  int level = 0;

Q_SIGNALS:
  void levelChanged();
};

/** @brief the gadget half of the pair, mirroring `twin_gadget`
 *
 * A `Q_GADGET` has no signals, so no property here can carry `NOTIFY`: moc
 * rejects that outright. It is the shape the metatypes document used to invent
 * a notify signal for.
 */
class mirror_gadget
{
  Q_GADGET
  QML_VALUE_TYPE(span)

  Q_PROPERTY(int length MEMBER length)
  Q_PROPERTY(unsigned int span_uint MEMBER span_uint)
  Q_PROPERTY(short span_short MEMBER span_short)
  Q_PROPERTY(unsigned short span_ushort MEMBER span_ushort)
  Q_PROPERTY(unsigned char span_uchar MEMBER span_uchar)
  Q_PROPERTY(signed char span_schar MEMBER span_schar)
  Q_PROPERTY(long span_long MEMBER span_long)
  Q_PROPERTY(unsigned long span_ulong MEMBER span_ulong)
  Q_PROPERTY(long long span_llong MEMBER span_llong)
  Q_PROPERTY(unsigned long long span_ullong MEMBER span_ullong)

public:
  Q_INVOKABLE int doubled() const
  {
    return length * 2;
  }

  int                length      = 0;
  unsigned int       span_uint   = 0;
  short              span_short  = 0;
  unsigned short     span_ushort = 0;
  unsigned char      span_uchar  = 0;
  signed char        span_schar  = 0;
  long               span_long   = 0;
  unsigned long      span_ulong  = 0;
  long long          span_llong  = 0;
  unsigned long long span_ullong = 0;

public Q_SLOTS:
  void stretch(int by = 1)
  {
    length += by;
  }
};
