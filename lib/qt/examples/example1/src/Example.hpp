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
  [[= prop<"rwn">]] QString subject;
  [[= prop<"rwn">]] QString body;
};

class                                          //
    [[= qt::classinfo{"QML.Element", "auto"}]] //
    [[= qt::classinfo{"Creatable", "true"}]]   //
    Example : public qt::object<Example>
{
public:
  Example(QObject* parent = nullptr) : qt::object<Example>{parent}
  {
    qt::dump<Message>();
    updateClock(); // initialize property
    startTimer<^^updateClock>(1000);
  }
  virtual ~Example() = default;

  [[= prop<"rwn">]] QString clockText = "00:00:00";

  signal<int, defaulted<int>> intSig{this, 42};
  signal<Message>             send{this};

  [[= slot]] void slot1(int value)
  {
    intSig(value);
  }

  [[= invocable]] bool sayTheTruth(bool lie = false)
  {
    send(Message{.subject = "Example", .body = QString("I'm%1lying !").arg(lie ? " " : " not ")});
    return lie;
  }

protected:
  [[= timer_event]] void updateClock()
  {
    setProperty<^^clockText>(QString::fromStdString(std::format("{:%X}", std::chrono::system_clock::now())));
  }
};