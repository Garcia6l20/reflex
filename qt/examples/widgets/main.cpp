#include <reflex/qt.hpp>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <QtCore/QString>

#include <format>

struct [[= reflex::qt::classinfo{"author", "reflex"}]] tally
    : reflex::qt::object<tally>
{
  friend reflex::qt::access<tally>;

  signal<int> changed{this};

  [[= slot]] void increment()
  {
    setProperty<^^count>(count + 1);
  }

  [[= slot]] void reset()
  {
    setProperty<^^count>(0);
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

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  window w;
  w.show();

  return QApplication::exec();
}
