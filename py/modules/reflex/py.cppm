// nanobind reaches into CPython's headers, which are neither modular nor
// willing to be imported. They belong in the global module fragment, where the
// declarations stay attached to the global module and remain usable from the
// purview without being exported.
module;

#include <nanobind/nanobind.h>

export module reflex.py;

export import reflex.core;

import std;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/py.hpp>
