export module test.tu.mod1;

import reflex.core;
import std;

import test.tu;

export namespace test
{
struct[[= registered_type{}]] test_s1 : auto_registered
{};

} // namespace test
