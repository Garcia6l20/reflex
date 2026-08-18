#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>

#include <cstddef>
#include <format>
#include <string_view>

/** @brief formats a `QString` as UTF-8, honouring the full string format spec
 *
 * Narrow only, deliberately. A `wchar_t` specialization would have to transcode
 * `QString`'s UTF-16 into a unit that is 4 bytes here and 2 bytes on Windows,
 * so its output would depend on the platform, and nothing in Qt hands out a
 * `wchar_t` view to borrow. Leaving the wide instantiation to the disabled
 * primary template turns `std::format(L"{}", s)` into an error where it is
 * written rather than into a formatted `const QChar*` pointer value.
 */
template <> struct std::formatter<QString, char> : std::formatter<std::string_view, char>
{
  auto format(QString const& s, auto& ctx) const
  {
    const QByteArray utf8 = s.toUtf8();
    return std::formatter<std::string_view, char>::format(
        std::string_view{utf8.constData(), std::size_t(utf8.size())},
        ctx);
  }
};
