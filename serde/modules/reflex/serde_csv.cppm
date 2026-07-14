module;

#include <cstring>

export module reflex.serde.csv;

import std;

export import reflex.core;
export import reflex.serde;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/serde/csv.hpp>
