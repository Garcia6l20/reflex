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
