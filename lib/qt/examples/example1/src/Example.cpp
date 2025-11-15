#include <Example.hpp>

Example::Example(QObject* parent) : qt::object<Example>{parent}
{
  qt::dump<Message>();
}

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

void Example::slot1(int value)
{
  intSig(value);
}

bool Example::sayTheTruth(bool lie)
{
  send(Message{.subject = "Example", .body = QString("I'm%1lying !").arg(lie ? " " : " not ")});
  return lie;
}

void Example::updateClock()
{
  setProperty<^^clockText>(QString::fromStdString(std::format("{:%X}", std::chrono::system_clock::now())));
}
