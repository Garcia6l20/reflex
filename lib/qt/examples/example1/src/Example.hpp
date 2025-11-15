#pragma once

#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>

using namespace reflex;

struct                                                                                           //
    [[= qt::classinfo{"QML.Element", "message"}]]                                                //
    [[= qt::classinfo{"Creatable", "false"}]]                                                    //
    [[= qt::classinfo{"UncreatableReason", "A message may only be emitted by Example objects"}]] //
    Message : qt::gadget<Message>
{
  [[= prop{}]] // "rwn" (read-write-notify) is the default
      QString          subject;
  [[= prop{}]] QString body;
};

class                                          //
    [[= qt::classinfo{"QML.Element", "auto"}]] //
    [[= qt::classinfo{"Creatable", "true"}]]   //
    Example : public qt::object<Example>
{
public:
  Example(QObject* parent = nullptr);
  virtual ~Example() = default;

  [[= prop{}]] QString              clockText = "00:00:00";
  [[= prop{}]] bool                 running   = false;
  [[= listener_of<^^running>]] void runningChanged();

  signal<int, defaulted<int>> intSig{this, /* here is the defaulted value */ 42};
  signal<Message>             send{this};

  [[= slot]] void slot1(int value);

  [[= invocable]] bool sayTheTruth(bool lie = false);

protected:
  [[= timer_event]] void updateClock();
};