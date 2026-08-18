#pragma once

#include <QtCore/qtversion.h>

#ifndef REFLEX_QT_ALLOW_UNTESTED_QT
static_assert(QT_VERSION >= QT_VERSION_CHECK(6, 11, 0) and QT_VERSION < QT_VERSION_CHECK(6, 12, 0),
              "reflex.qt reproduces moc's private metaobject layout and is only tested against "
              "Qt 6.11.x; define REFLEX_QT_ALLOW_UNTESTED_QT to try anyway");
#endif
