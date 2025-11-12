#pragma once

#include <reflex/qt.hpp>

using namespace reflex;

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
    updateClock(); // initialize property
    startTimer<^^updateClock>(1000);
  }
  virtual ~Example() = default;

  [[= prop<"rwn">]] QString clockText = "00:00:00";

  signal<int, defaulted<int>> intSig{this, 42};

  [[= slot]] void slot1(int value)
  {
    intSig(value);
  }

  [[= invocable]] bool sayTheTruth(bool lie = false)
  {
    std::println("Example: I'm{}lying !", lie ? " " : " not ");
    return lie;
  }

protected:
  [[= timer_event]] void updateClock()
  {
    setProperty<^^clockText>(QString::fromStdString(std::format("{:%X}", std::chrono::system_clock::now())));
  }
};