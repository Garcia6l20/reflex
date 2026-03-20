#pragma once

#include "reg-mod.hpp"

namespace test
{
struct[[= registered_type{}]] test_s1 : auto_registered
{};

} // namespace test
