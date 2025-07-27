#pragma once

#include <reflex/meta.hpp>

#include <QtCore/qmetatype.h>

namespace reflex::qt::detail
{
static constexpr auto     custom_type = std::bit_cast<QMetaType::Type>(0x8000'0000);
consteval QMetaType::Type static_meta_type_id_of(meta::info R)
{
  if(false)
  {
  }
#define X(__enum_mem, __id, __type)                                        \
  else if(remove_const(remove_reference(dealias(^^__type))) == dealias(R)) \
  {                                                                        \
    return QMetaType::__enum_mem;                                          \
  }
  QT_FOR_EACH_STATIC_TYPE(X)
#undef X
  return custom_type;
}
} // namespace reflex::qt::detail
