#pragma once

#include <QtCore/qtversion.h>

#ifndef REFLEX_QT_ALLOW_UNTESTED_QT
static_assert(QT_VERSION >= QT_VERSION_CHECK(6, 10, 0) and QT_VERSION < QT_VERSION_CHECK(6, 12, 0),
              "reflex.qt reproduces moc's private metaobject layout and is only tested against "
              "Qt 6.10.x and 6.11.x; configure with REFLEX_QT_ALLOW_UNTESTED_QT=true to try anyway");
#endif
