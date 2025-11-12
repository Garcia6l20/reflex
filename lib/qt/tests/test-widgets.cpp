#include <reflex/qt.hpp>
#include <reflex/qt/dump.hpp>

#include <SimpleWidget.hpp>

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

// local moc instanciation
#include <reflex/qt/detail/object_impl.hpp>
REFLEX_QT_OBJECT_IMPL(WidgetTests)

QTEST_MAIN(WidgetTests)
