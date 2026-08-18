#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

/** @file
 *
 * The moc side of the metatypes cross-check: a hand-written `Q_OBJECT` class
 * whose members match `twin` in `moc-pair.hpp` one for one, in the same order.
 * `moc --output-json` on this header and `write_metatypes` on `twin` must
 * produce the same document; `moc-cross-check.py` diffs them.
 *
 * moc never runs on it in the build. `test-moc-json.cpp` includes it so a change
 * to `twin` cannot leave the mirror behind as invalid C++.
 */
class mirror : public QObject
{
  Q_OBJECT
  Q_CLASSINFO("QML.Element", "auto")
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
