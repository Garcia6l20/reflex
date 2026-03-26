export module reflex.serde.json;

export import reflex.serde;

import std;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/serde/json.hpp>
