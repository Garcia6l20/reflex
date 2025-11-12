#pragma once

#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include <reflex/qt.hpp>

using namespace reflex;

class SimpleWidget : public qt::object<SimpleWidget, QWidget>
{
public:
  SimpleWidget(QWidget* parent = nullptr) : qt::object<SimpleWidget, QWidget>{parent}
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