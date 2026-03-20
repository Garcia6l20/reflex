#pragma once

#include <pipe_capture.hpp>

namespace reflex::testutils
{
  bool set_env(const char* name, const char* value, int overwrite);
  bool unset_env(const char* name);
} // namespace reflex::testutils
