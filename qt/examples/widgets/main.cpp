#include <reflex/qt.hpp>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <QtCore/QString>

#include <format>

namespace qt = reflex::qt;

struct [[= qt::classinfo{"author", "reflex"}]] tally : qt::object<tally>
{
  friend qt::access<tally>;

  signal<int> changed{this};

  [[= qt::slot]] void increment()
  {
    setProperty<^^count>(count + 1);
  }

  [[= qt::slot]] void reset()
  {
    setProperty<^^count>(0);
  }

  [[= qt::invocable]] QString caption() const
  {
    return QString::fromStdString(std::format("clicked {} time{}", count, count == 1 ? "" : "s"));
  }

private:
  [[= qt::prop{}]] int count = 0;

  [[= qt::listener<^^count>]] void onCountChanged()
  {
    changed(count);
  }
};

class window : public qt::object<window, QWidget>
{
  friend qt::access<window>;

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
  [[= qt::slot]] void refresh(int)
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
