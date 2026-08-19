#pragma once

#include <QtCore/qobject.h>
#include <QtQmlIntegration/qqmlintegration.h>

/** @file
 *
 * The moc side of the QML class-info check: one hand-written class per
 * `QML_*` macro combination `test-qml.cpp` exercises through `qt::qml`.
 * moc runs on this header in the build and its output is linked into
 * `reflex-test-qt-qml`, so `test-qml.cpp` reads real moc's class infos
 * out of a real `staticMetaObject` and compares them against reflex's.
 */
class qml_plain : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(int value MEMBER value)

public:
  int value = 0;
};

class qml_named : public QObject
{
  Q_OBJECT
  QML_NAMED_ELEMENT(Gauge)
};

class qml_lone : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
};

class qml_sealed : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("ask the factory")
};

class qml_versioned
{
  Q_GADGET
  QML_VALUE_TYPE(span)
  QML_ADDED_IN_VERSION(2, 3)
  QML_REMOVED_IN_VERSION(3, 0)
};

class qml_mixed : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  Q_CLASSINFO("author", "reflex")
};

class qml_none : public QObject
{
  Q_OBJECT
};
