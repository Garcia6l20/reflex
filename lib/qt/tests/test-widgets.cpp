#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>

namespace reflex::qt
{
template <typename Super> using widget = object<Super, QWidget>;
};

using namespace reflex;

class SimpleWidget : public qt::widget<SimpleWidget>
{
public:
  SimpleWidget(QWidget* parent = nullptr) : qt::widget<SimpleWidget>{parent}
  {
    auto* l   = new QVBoxLayout{this};
    auto* btn = new QPushButton{"click me !"};
    btn->setCheckable(true);
    l->addWidget(btn);
    connect(btn, &QPushButton::clicked, this, &SimpleWidget::onBtnClick);
  }

  bool clicked = false;

  signal<bool> btnClicked{this};

private:
  [[= slot]] void onBtnClick(bool on)
  {
    std::println("btnClicked !!");
    clicked = true;
    btnClicked(on);
  }
};

class WidgetTests : public qt::object<WidgetTests>
{
  [[= slot]] void testButtonClick()
  {
    SimpleWidget w;
    qt::dump(w);

    QSignalSpy spy{&w, &SimpleWidget::btnClicked};
    QTest::mouseClick(w.findChildren<QPushButton*>()[0], Qt::LeftButton, Qt::NoModifier);
    QVERIFY(spy.count() == 1);
    QVERIFY(spy.takeFirst().at(0).toBool() == true);
  }
};

QTEST_MAIN(WidgetTests)
