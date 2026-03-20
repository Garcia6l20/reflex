#pragma once

#include <reflex/core.hpp>

namespace test
{
struct registered_type
{};

using annotated_registry = reflex::meta::reg::annotated_type_registry<^^test, ^^registered_type>;

struct auto_registered
{};

using derived_registry = reflex::meta::reg::derived_type_registry<^^test, ^^auto_registered>;

} // namespace test
