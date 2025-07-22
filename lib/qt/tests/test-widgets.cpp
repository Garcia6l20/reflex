#include <QPushButton>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include <reflex/qt.hpp>

namespace reflex::qt {
  template <typename Super>
  using widget = object<Super, QWidget>;
};

using namespace reflex;

class SimpleWidget : public qt::widget<SimpleWidget>
{
public:
  SimpleWidget(QWidget* parent = nullptr) : qt::widget<SimpleWidget>{parent}
  {
    auto* l   = new QVBoxLayout{this};
    auto* btn = new QPushButton{"click me !"};
    l->addWidget(btn);
    connect(btn, &QPushButton::clicked, this, &SimpleWidget::btnClicked);
  }

  bool clicked = false;

private:
  [[= qt::slot]] void btnClicked()
  {
    std::println("btnClicked !!");
    clicked = true;
  }
};

class WidgetTests : public qt::object<WidgetTests>
{
  [[= qt::slot]] void testButtonClick()
  {
    SimpleWidget w;
    QTest::mouseClick(w.findChildren<QPushButton*>()[0], Qt::LeftButton, Qt::NoModifier);
    QVERIFY(w.clicked == true);
  }
};

QTEST_MAIN(WidgetTests)
