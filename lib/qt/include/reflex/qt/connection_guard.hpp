#pragma once

#include <QMetaObject>
#include <QObject>

namespace reflex::qt
{
struct connection_guard
{
  connection_guard() = default;
  connection_guard(QMetaObject::Connection&& con) noexcept : con_{std::move(con)}
  {
  }
  ~connection_guard()
  {
    if(con_)
    {
      QObject::disconnect(con_);
    }
  }

private:
  QMetaObject::Connection con_;
};
} // namespace reflex::qt
