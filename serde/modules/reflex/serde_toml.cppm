module;

#include <cstring>

export module reflex.serde.toml;

import std;

export import reflex.core;
export import reflex.serde;
export import reflex.poly;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/serde/toml_value.hpp>

#include <reflex/serde/toml.hpp>
