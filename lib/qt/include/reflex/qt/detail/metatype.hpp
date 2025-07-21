#pragma once

#include <reflex/meta.hpp>

#include <QtCore/qmetatype.h>

namespace reflex::qt::detail
{
consteval QMetaType::Type static_meta_type_id_of(meta::info R)
{
  if(false)
  {
  }
#define X(__enum_mem, __id, __type)        \
  else if(dealias(^^__type) == dealias(R)) \
  {                                        \
    return QMetaType::__enum_mem;          \
  }
  QT_FOR_EACH_STATIC_TYPE(X)
#undef X
  std::unreachable();
}
} // namespace reflex::qt::detail
