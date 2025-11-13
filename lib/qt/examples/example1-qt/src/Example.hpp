#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

struct Message
{
  Q_GADGET
  QML_VALUE_TYPE(message)
  QML_UNCREATABLE("Just because")

  Q_PROPERTY(QString subject MEMBER subject)
  Q_PROPERTY(QString body MEMBER body)
public:

  QString subject;
  QString body;
};

class Example : public QObject
{
  Q_OBJECT
  QML_ELEMENT

public:
  Example(QObject* parent = nullptr) : QObject{parent}
  {
  }
  virtual ~Example() = default;

Q_SIGNALS:
  void intSig(int);
  void send(Message const&);

public Q_SLOTS:
  void slot1(int value)
  {
    intSig(value);
  }

  Q_INVOKABLE bool sayTheTruth(bool lie = false)
  {
    Message msg;
    msg.subject = "Example";
    msg.body = QString("I'm%1lying !").arg(lie ? " " : " not ");
    send(msg);
    return lie;
  }

protected:
};