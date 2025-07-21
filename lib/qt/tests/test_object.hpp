#pragma once

#include <QObject>
#include <QDebug>

class QTestObject : public QObject
{
  Q_OBJECT
  Q_PROPERTY(int intProperty READ intValue WRITE intSlot NOTIFY intChanged)

public:

  QTestObject()          = default;
  virtual ~QTestObject() = default;

  int intValue()
  {
    return 42;
  }

signals:
  void emptySig();
  void emptySig2();
  void intSig(int);
  void intChanged();

public slots:
  void emtpySlot()
  {
    qDebug() << "emtpySlot called";
  }
  void intSlot(int value)
  {
    qDebug() << "intSlot called with" << value;
    emit intChanged();
  }
//   // void setRefProperty(RefEnum value)
//   // {
//   //   value_ = value;
//   //   emit refPropertyChanged(value_);
//   // }

// private:
//   // RefEnum value_;
//   int ivalue_ = 0;
};
