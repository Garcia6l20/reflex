#pragma once

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
  Q_PROPERTY(mirror::Mode mode MEMBER mode NOTIFY modeChanged FINAL REQUIRED)

public:
  enum Mode
  {
    Fast,
    Slow
  };
  Q_ENUM(Mode)

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

Q_SIGNALS:
  void bumped(int amount);
  void countChanged();
  void extraChanged();
  void modeChanged();

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

public:
  Q_INVOKABLE int doubled() const
  {
    return length * 2;
  }

  int length = 0;
};
