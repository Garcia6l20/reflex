#pragma once

#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>

using namespace reflex;

struct Message : qt::gadget<Message>
{
  struct ClassInfos
  {
    struct QML
    {
      static constexpr auto Element           = "message";
      static constexpr auto Creatable         = "false";
      static constexpr auto UncreatableReason = "Just because";
    };
  };

  [[= prop<"rwn">]] QString subject;
  [[= prop<"rwn">]] QString body;
};

class Example : public qt::object<Example>
{
  struct ClassInfos
  {
    struct QML
    {
      static constexpr auto Element   = "auto";
      static constexpr auto Creatable = "true";
    };
  };

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
  signal<Message> send{this};

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