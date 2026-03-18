# reflex.core

> **C++26 reflection utilities** - the foundation layer for the `reflex` library family.

`reflex.core` provides building blocks that every other `reflex` module depends on:
compile-time metadata helpers, type-safe visitation, pattern matching, string utilities,
and a rich set of concepts & type traits.

---

## Highlights

### `reflex::visit` + `reflex::match`

A reflection-powered replacement for `std::visit` that works with `std::variant`
subclasses, objects with a `.visit()` member, or any visitable type — without
needing to spell out the index manually.

```cpp
import reflex.core;
using namespace reflex;

std::variant<int, std::string, float> v = 42;

visit(match{
    [](int    i) { std::println("int:    {}", i); },
    [](float  f) { std::println("float:  {}", f); },
    [](auto const& s) { std::println("other:  {}", s); },
}, v);

// Pipe syntax also works:
v | match{
    [](int i)  { return i * 2; },
    [](auto&)  { return -1;    },
};
```

### `reflex::meta` - annotation & reflection helpers

```cpp
import reflex.core;

// Find member by name at compile time
consteval auto mem = reflex::meta::member_named(^^MyStruct, "field_name");

// Typed annotation extraction
constexpr auto ann = reflex::meta::annotation_value_of_with<MyAnnotation>(mem);

// Enum ↔ string
auto s = reflex::meta::enum_to_string(MyEnum::Value); // → "Value"
auto e = reflex::meta::string_to_enum<MyEnum>("Value"); // → std::optional<MyEnum>
```

### `reflex::parse<T>`

```cpp
auto i = reflex::parse<int>("42");          // → 42
auto f = reflex::parse<float>("3.14");      // → 3.14f
auto s = reflex::parse<std::string>("hi");  // → "hi"
```

### `constant_string` - NTTP-compatible strings

```cpp
constexpr reflex::constant_string cs{"hello"};
static_assert(cs.view() == "hello");
// Usable as non-type template parameter
```

---

> See [tests](tests) for more examples.

---

## Modules

| Module partition | Exported as | Purpose |
|---|---|---|
| `core.cppm` | `reflex.core` | Umbrella re-export of all partitions |
| `meta.cppm` | `:meta` | `std::meta` helpers & annotation utilities |
| `visit.cppm` | `:visit` | `reflex::visit` for variants & visitable types |
| `match.cppm` | `:match` | `reflex::match` overload set + `patterns::` |
| `concepts.cppm` | `:concepts` | `seq_c`, `map_c`, `str_c`, `number_c`, … |
| `constant_string.cppm` | `:constant_string` | Compile-time string type usable as NTTP |
| `parse.cppm` | `:parse` | Generic `reflex::parse<T>(string_view)` |
| `utils.cppm` | `:utils` | `ltrim`, `rtrim`, `trim`, string helpers |
| `formatters.cppm` | `:formatters` | `std::formatter` specialisations |
| `named_arg.cppm` | `:named_arg` | Named-argument helpers |
| `exception.cppm` | `:exception` | `reflex::exception` with `std::format` support |
| `caseconv.cppm` | `:caseconv` | `snake_case` ↔ `camelCase` conversions |
| `to_tuple.cppm` | `:to_tuple` | Aggregate → `std::tuple` via reflection |
| `const_assert.cppm` | `:const_assert` | Improved `constexpr`-friendly assertions |
| `views/` | `:views.*` | Custom range adaptors (cartesian product, …) |
