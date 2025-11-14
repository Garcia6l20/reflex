#pragma once

#include <reflex/qt.hpp>

using namespace reflex;

class                                          //
    [[= qt::classinfo{"QML.Element", "auto"}]] //
    [[= qt::classinfo{"Creatable", "true"}]]   //
    ClockController : public qt::object<ClockController>
{
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
