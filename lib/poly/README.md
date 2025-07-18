# reflex.poly

Polymorphic library for c++26.

## Types

- `reflex::var<...>`: an `std::variant<...>` alternative:
```cpp
using var = reflex::var<bool, int, std::string>;
var v = 42;
assert(v.has_value<int>());
assert(v == 42);
v = true;
assert(v.has_value<bool>());
assert(v == true);
v = "hello world"s; // no implicit conversion... std::string required here (maybe handled in the future)
assert(v == "hello world"s); // std::string required for now (will be handled in soon future)
```

- `reflex::recursive_var<...>`: a recursive version of `reflex::var<...>`:
```cpp
using var = reflex::recursive_var<bool, int, std::string>;
var v = 42;
assert(v.has_value<int>());
assert(v == 42);
v = true;
assert(v.has_value<bool>());
assert(v == true);
v = "hello world"s; // no implicit conversion... std::string required here (maybe handled in the future)
assert(v == "hello world"s); // std::string required for now (will be handled in soon future)

v = var::vec_t{1, "hello"s, true};
assert(v.has_value<var::vec_t>());
assert(v.has_value<std::vector>());
assert(v[0] == 1);
assert(v[1] == "hello"s);
assert(v[2] == true);
for (auto const& i : v.vec()) {
  i.match([](auto const& value) { std::println("value is {} (type={})", value, display_string_of(type_of(^^value))); });
}
std::println("vec = {}", v);

v = var::map_t{{"a", 1}, {"b": "hello"s}, {"c", true}};
assert(v.has_value<var::map_t>());
assert(v.has_value<std::map>());
assert(v["a"] == 1);
assert(v["b"] == "hello"s);
assert(v["c"] == true);
for (auto const& [k, i] : v.map()) {
  i.match([&k](auto const& value) { std::println("value '{}' is {} (type={})", k, value, display_string_of(type_of(^^value))); });
}
std::println("map = {}", v);
```
