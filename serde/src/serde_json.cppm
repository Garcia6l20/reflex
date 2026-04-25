export module reflex.serde.json;

import std;

export import reflex.core;
export import reflex.serde;
export import reflex.poly;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/serde/json_value.hpp>

#include <reflex/serde/json.hpp>
