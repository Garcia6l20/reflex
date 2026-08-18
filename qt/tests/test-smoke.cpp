#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QObject>

TEST_CASE("qt is linked and the version guard passes")
{
  QObject o;
  o.setObjectName("smoke");
  CHECK(o.objectName() == QStringLiteral("smoke"));
  CHECK(o.metaObject()->className() == std::string_view{"QObject"});
}
