export module reflex.serde.bson;

import std;

export import reflex.serde;
export import reflex.poly;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/serde/bson_value.hpp>

#include <reflex/serde/bson.hpp>
