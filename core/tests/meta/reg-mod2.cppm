export module test.tu.mod2;

import reflex.core;
import std;

import test.tu;

export namespace test
{
struct test_s2 : auto_registered
{};

struct auto_registered2 : auto_registered
{};

template <typename T> struct register_type : auto_registered
{
  using type = T;
};

struct[[= reflex::meta::reg::type_provider{}]] auto_registered3
{
  using type = register_type<int>; // type will be registered as a nested type of auto_registered3
};

template <typename T> struct[[= reflex::meta::reg::type_provider{}]] register_type_wrapper
{
  using type = register_type<T>;
};

template <typename... Ts>
struct[[= reflex::meta::reg::type_provider{.subclass_lookup = true}]] register_all
    : register_type_wrapper<Ts>...
{};

using some_types = register_all<double, std::string>;

using some_other_types = derived_registry::register_<float, std::vector<int>>;

} // namespace test
