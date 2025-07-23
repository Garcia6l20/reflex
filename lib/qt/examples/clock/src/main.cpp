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
    updateClock(); // initialize property
    startTimer<^^updateClock>(1000);
  }
  virtual ~Controller() = default;

  [[= prop<"rwn">]] QString clockText = "00:00:00";

protected:
  [[= timer_event]] void updateClock()
  {
    setProperty<^^clockText>(QString::fromStdString(std::format("{:%X}", std::chrono::system_clock::now())));
  }
};

int main(int argc, char** argv)
{
  QGuiApplication app(argc, argv);
  app.setOrganizationName("reflex");
  app.setOrganizationDomain("examples");
  app.setApplicationName("clock-example");
  QQmlApplicationEngine engine;
  auto*                 controller = new Controller{&engine};

  qt::dump(controller);

  auto& ctx = *engine.rootContext();
  ctx.setContextProperty("controller", controller);
  engine.load(QUrl(QStringLiteral("qrc:/q/Clock/qml/Clock/main.qml")));
  return app.exec();
}