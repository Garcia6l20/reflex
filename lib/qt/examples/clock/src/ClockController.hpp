#pragma once

#include <reflex/qt.hpp>

using namespace reflex;

class ClockController : public qt::object<ClockController>
{
  struct ClassInfos
  {
    struct QML
    {
      static constexpr auto Element = "auto";
      static constexpr auto Creatable = "true";
    };
  };

public:
  ClockController(QObject* parent = nullptr) : qt::object<ClockController>{parent}
  {
    updateClock(); // initialize property
    startTimer<^^updateClock>(1000);
  }
  virtual ~ClockController() = default;

  [[= prop<"rwn">]] QString clockText = "00:00:00";

protected:
  [[= timer_event]] void updateClock()
  {
    setProperty<^^clockText>(QString::fromStdString(std::format("{:%X}", std::chrono::system_clock::now())));
  }
};
