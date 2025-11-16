#include <Example.hpp>

namespace reflex
{

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

Example::TruthStyle Example::sayTheTruth(TruthStyle style)
{
  send(
      Message{.subject = "Example",
              .body = QString("I'm %1 lying !").arg(style == TruthStyle::Trump ? "never" : "not")});
  return style;
}

void Example::updateClock()
{
  setProperty<^^clockText>(
      QString::fromStdString(std::format("{:%X}", std::chrono::system_clock::now())));
}

} // namespace reflex
