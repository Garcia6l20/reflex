#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qobjectdefs.h>

#include <utility>

namespace reflex::qt
{
/** @brief disconnects the connection it holds when it goes out of scope
 *
 * ```cpp
 * reflex::qt::connection_guard guard = QObject::connect(&sender, &s::ping, [] {});
 * ```
 *
 * Move-only: a copy would disconnect the same connection twice. `release()`
 * hands the connection out and leaves the guard empty, `reset()` drops what it
 * holds and optionally takes another connection.
 */
class connection_guard
{
public:
  connection_guard() noexcept = default;

  connection_guard(QMetaObject::Connection connection) noexcept
      : connection_{std::move(connection)}
  {
  }

  connection_guard(connection_guard const&)            = delete;
  connection_guard& operator=(connection_guard const&) = delete;

  connection_guard(connection_guard&& other) noexcept : connection_{other.release()}
  {
  }

  connection_guard& operator=(connection_guard&& other) noexcept
  {
    if(this != &other)
    {
      reset(other.release());
    }
    return *this;
  }

  ~connection_guard()
  {
    reset();
  }

  /** @brief Hands the connection out, leaving the guard empty. */
  [[nodiscard]] QMetaObject::Connection release() noexcept
  {
    return std::exchange(connection_, QMetaObject::Connection{});
  }

  /** @brief Disconnects what the guard holds, then takes @p connection. */
  void reset(QMetaObject::Connection connection = {}) noexcept
  {
    if(connection_)
    {
      QObject::disconnect(connection_);
    }
    connection_ = std::move(connection);
  }

  [[nodiscard]] QMetaObject::Connection const& get() const noexcept
  {
    return connection_;
  }

  explicit operator bool() const noexcept
  {
    return bool(connection_);
  }

private:
  QMetaObject::Connection connection_;
};
}
