#include <QtCore/qcoreapplication.h>
#include <QtQml/qqmlapplicationengine.h>

#include <cstdio>

int main(int argc, char** argv)
{
  qInstallMessageHandler([](QtMsgType, QMessageLogContext const&, QString const& message)
                         { std::printf("%s\n", qPrintable(message)); });

  QCoreApplication      app(argc, argv);
  QQmlApplicationEngine engine;
  engine.loadFromModule("Reflex.Demo", "Main");
  if(engine.rootObjects().isEmpty())
  {
    return EXIT_FAILURE;
  }
  return app.exec();
}
