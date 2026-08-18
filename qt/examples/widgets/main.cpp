#include <reflex/qt.hpp>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <QtCore/QMetaObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>
#include <string_view>

struct [[= reflex::qt::classinfo{"author", "reflex"}]] tally
    : reflex::qt::object<tally>
{
  friend reflex::qt::access<tally>;

  signal<int> changed{this};

  [[= slot]] void increment()
  {
    setProperty<"count">(count + 1);
  }

  [[= slot]] void reset()
  {
    setProperty<"count">(0);
  }

  [[= invocable]] QString caption() const
  {
    return QString::fromStdString(std::format("clicked {} time{}", count, count == 1 ? "" : "s"));
  }

private:
  [[= prop{}]] int count = 0;

  [[= listener<^^count>]] void onCountChanged()
  {
    changed(count);
  }
};

class window : public reflex::qt::object<window, QWidget>
{
  friend reflex::qt::access<window>;

public:
  window()
  {
    setWindowTitle(QStringLiteral("reflex.qt widgets"));

    auto* const layout = new QVBoxLayout(this);
    layout->addWidget(label_);
    layout->addWidget(button_);

    connect(button_, &QPushButton::clicked, &tally_, &tally::increment);
    connect(&tally_, &tally::changed, this, &window::refresh);

    refresh(0);
  }

  tally& counter()
  {
    return tally_;
  }

  QPushButton& button()
  {
    return *button_;
  }

  QString caption() const
  {
    return label_->text();
  }

private:
  [[= slot]] void refresh(int)
  {
    label_->setText(tally_.caption());
  }

  tally        tally_;
  QLabel*      label_  = new QLabel(this);
  QPushButton* button_ = new QPushButton(QStringLiteral("Click me"), this);
};

namespace
{
int check(bool ok, std::string_view what)
{
  if(not ok)
  {
    std::println(stderr, "FAILED: {}", what);
  }
  return ok ? 0 : 1;
}
}

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  window w;
  w.show();

  int failures = 0;
  failures += check(w.caption() == QStringLiteral("clicked 0 times"), "initial caption");

  w.button().click();
  failures += check(w.caption() == QStringLiteral("clicked 1 time"), "caption after one click");

  w.button().click();
  failures += check(w.caption() == QStringLiteral("clicked 2 times"), "caption after two clicks");

  failures += check(w.counter().property("count").toInt() == 2, "count through QVariant");
  failures += check(QMetaObject::invokeMethod(&w.counter(), "reset"), "reset through invokeMethod");
  failures += check(w.caption() == QStringLiteral("clicked 0 times"), "caption after reset");

  std::println("{}", w.caption().toStdString());
  std::println("tally publishes {} methods and {} property, window {} methods, no Q_OBJECT anywhere",
               tally::staticMetaObject.methodCount() - tally::staticMetaObject.methodOffset(),
               tally::staticMetaObject.propertyCount() - tally::staticMetaObject.propertyOffset(),
               window::staticMetaObject.methodCount() - window::staticMetaObject.methodOffset());

  QTimer::singleShot(0, &app, &QCoreApplication::quit);
  const int code = QApplication::exec();

  return failures == 0 ? code : EXIT_FAILURE;
}
