#include <QCommandLineOption>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>

#include <chrono>

using namespace reflex;

class Controller : public qt::object<Controller>
{
public:
  Controller(QObject* parent = nullptr) : qt::object<Controller>{parent}
  {
    timerId_ = startTimer(1000);
  }
  virtual ~Controller() = default;

  [[= qt::property<"rwn">]] QString clockText = "00:00:00";

protected:
  void timerEvent(QTimerEvent* e) final
  {
    if(e->timerId() == timerId_)
    {
      // use setProperty to trigger notification signal automaticaly
      setProperty<^^clockText>(QString::fromStdString(std::format("{:%X}", std::chrono::system_clock::now())));
    }
  }

private:
  int timerId_ = -1;
};

int main(int argc, char** argv)
{
  QGuiApplication app(argc, argv);
  app.setOrganizationName("tpl");
  app.setOrganizationDomain("b2r");
  app.setApplicationName("b2r-configurator");
  QQmlApplicationEngine engine;
  auto*                 controller = new Controller{&engine};

  qt::dump(*controller);

  auto& ctx = *engine.rootContext();
  ctx.setContextProperty("controller", controller);
  engine.load(QUrl(QStringLiteral("qrc:/q/Clock/qml/Clock/main.qml")));
  return app.exec();
}