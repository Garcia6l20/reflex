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

### `constant<...>` - non-structural compile-time constants

> The constant API is derived from the [ctp](https://github.com/brevzin/ctp) library by Barry Revzin, with modifications for integration into this project. See [THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES) for details.

```cpp
constexpr reflex::constant_string cs{"hello"};
static_assert(cs == "hello");

// Usable as NTTP:
template <constant_string Str>
struct use_nttp_string
{
  static_assert(Str == "hello");
};
using example_nttp_string = use_nttp_string<"hello">;

// Useful for annotations:
struct my_annotation
{
  reflex::constant_string name;
  int value;
};

[[= my_annotation{"example", 42}]] struct annotated_struct {};

// Also with sequences:
template <constant<std::vector<int>> Seq> struct use_nttp_sequence {
  static_assert(Seq->at(0) == 1);
  static_assert(Seq->at(1) == 2);
  static_assert(Seq->at(2) == 3);
};

using example_nttp_sequence = use_nttp_sequence<std::vector{1, 2, 3}>;

// Constant wrapper:
auto use_nttp_string = []<auto S>(constant_wrapper<S> c) {
  static_assert(c.value == "hello");
  static_assert(c->data()[0] == 'h');
  static_assert(std::addressof(c.value) == std::addressof(S.value)); // points to the same object
};
use_nttp_string(cw<"hello"s>);
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
