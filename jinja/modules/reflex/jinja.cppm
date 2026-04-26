export module reflex.jinja;

export import reflex.core;
export import reflex.poly;
export import reflex.serde;
export import reflex.serde.json;

import std;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/jinja.hpp>
