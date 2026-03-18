# reflex.poly

> **Reflection-powered polymorphic value type for C++26.**

`reflex.poly` provides `reflex::poly::var<Ts...>` - a `std::variant`-based
container that understands objects, arrays, dotted-path access, reference
wrapping, and numeric type promotion.

---

## `poly::var<Ts...>`

A recursive variant that can hold:

| Alternative | Description |
|---|---|
| `null_t` | Null / empty state (`reflex::poly::null`) |
| `Ts...` | Your value types (e.g. `bool`, `int`, `double`, `std::string`) |
| `arr<Ts...>` | `std::vector<var<Ts...>>` — typed array |
| `obj<Ts...>` | `std::map<string, var<Ts...>>` — typed object / map |
| `T*` / pointer forms | Reference wrapping via `std::reference_wrapper<T>` |

### Typical alias

```cpp
import reflex.poly;
using namespace reflex::poly;

// A JSON-like value type
using value = var<bool, std::int64_t, double, std::string>;
using array  = value::arr_type;   // arr<bool, std::int64_t, double, std::string>
using object = value::obj_type;   // obj<bool, std::int64_t, double, std::string>
```

---

## Examples

### Construction

```cpp
value v1 = 42;           // integral — promoted to int64_t
value v2 = 3.14;         // floating point
value v3 = "hello"s;     // string
value v4 = null;         // null
value v5 = {             // object (initializer-list syntax)
    {"key", 1},
    {"name", "Alice"s},
};
value v6 = array{1, 2, 3};  // array
```

### Type predicates

```cpp
v1.template is<std::int64_t>(); // true
v4.is_null();                   // true
v5.is_object();                 // true
v6.is_array();                  // true
```

### Typed access

```cpp
auto n = v1.template as<std::int64_t>(); // throws std::bad_variant_access on mismatch
auto s = v3.template get<std::string>(); // → std::optional<std::string&>

v5["key"];                               // object subscript — returns var&
v5.at("key");                            // → std::optional<var const&>  (non-throwing)
v5.contains("missing");                  // → false
```

### Dotted-path access

```cpp
value cfg = {{"db", {{"host", "localhost"s}, {"port", 5432}}}};
cfg["db.host"];          // → var holding "localhost"
cfg.contains("db.port"); // → true
```

### Visitation

```cpp
import reflex.core;   // for reflex::visit / reflex::match

v1.visit(reflex::match{
    [](std::int64_t i) { std::println("int: {}", i);  },
    [](std::string& s) { std::println("str: {}", s);  },
    [](auto const&)    { std::println("other");        },
});
```

### `std::format` support

```cpp
std::println("{}", v5);   // {key:1,name:Alice}
std::println("{}", v6);   // [1,2,3]
std::println("{}", null); // null
```

### Reference wrapping

```cpp
std::string str = "hello";
value ref = std::ref(str);                   // holds std::string*
ref.template is<std::string&>();             // true
ref.template as<std::string&>() += " world"; // str == "hello world"
```

---

> See [tests](tests) for more examples.
