#pragma once

#include <QDebug>
#include <QObject>
#include <QProperty>
#include <QQmlEngine>

class TestType
{
public:
  TestType()                              = default;
  ~TestType()                             = default;
  TestType(const TestType&)            = default;
  TestType& operator=(const TestType&) = default;

};
Q_DECLARE_METATYPE(TestType);

class TestMessage
{
public:
  TestMessage()                              = default;
  ~TestMessage()                             = default;
  TestMessage(const TestMessage&)            = default;
  TestMessage& operator=(const TestMessage&) = default;

  TestMessage(const QString& body, const QStringList& headers) : m_body{body}, m_headers{headers}
  {
  }

  QString const& body() const
  {
    return m_body;
  }
  QStringList headers() const
  {
    return m_headers;
  }

private:
  QString     m_body;
  QStringList m_headers;
};
Q_DECLARE_METATYPE(TestMessage);

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

  Q_INVOKABLE bool sayTheTruth()
  {
    return true;
  }

  void doSendMessage(TestMessage const& m)
  {
    emit sendMessage(m);
  }

signals:
  void emptySig();
  // void emptySig2();
  void intSig(int value, int value2 = 42);
  void intChanged();
  void sendMessage(TestMessage const&);

public slots:
  void emtpySlot()
  {
    qDebug() << "emtpySlot called";
  }
  void intSlot(int value = 42, int value2 = 0)
  {
    qDebug() << "intSlot called with" << value << "and" << value2;
    emit intChanged();
  }

  void handleMessage(TestMessage const& message)
  {
    qDebug() << "message.body =" << message.body();
    qDebug() << "message.headers =" << message.headers();
  }

  void handleQmlEngine(QQmlEngine *engine)
  {
  }

public:
  QProperty<int> intProp{42};
};
