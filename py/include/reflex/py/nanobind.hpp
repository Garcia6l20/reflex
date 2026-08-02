/** @file
 * @brief the one place nanobind is included
 *
 * Every other header in this module reaches nanobind through here, so the
 * include order and the namespace alias are settled once.
 */
#pragma once

#include <nanobind/nanobind.h>

namespace reflex::py
{
  namespace nb = nanobind;
}

// The whole nanobind namespace is hidden, so anything of ours holding one of its
// types has to be too, or -Wattributes fires on every such member. An extension
// is compiled with -fvisibility=hidden anyway; this makes it hold regardless.
#define REFLEX_PY_HIDDEN NB_EXPORT_SHARED
