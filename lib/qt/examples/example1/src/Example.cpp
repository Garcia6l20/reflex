#include <Example.hpp>

void Example::runningChanged()
{
  if(running)
  {
    updateClock(); // update now
    startTimer<^^updateClock>(1000);
  }
  else
  {
    killTimer<^^updateClock>();
  }
}
