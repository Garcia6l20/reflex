# reflex.serde

> **`ser`ialization / `de`serialization for C++26.**

`reflex.serde` lets you serialize and deserialize C++ aggregates, standard
containers, and `reflex::poly::var` values without writing any boilerplate -
field names, nesting, and type dispatch are all derived automatically via
C++26 static reflection.

Ships with **JSON** (`reflex.serde.json`) and **BSON** (`reflex.serde.bson`)
backends.

---

## Modules

| Module | Purpose |
|---|---|
| `reflex.serde` | Core concepts, annotations, `object_visit` |
| `reflex.serde.json` | JSON serializer + deserializer |
| `reflex.serde.bson` | BSON serializer + deserializer |

---

## Quick start

```cpp
import reflex.serde.json;
using namespace reflex::serde;

struct person
{
  std::string name;
  int         age    = 0;
  bool        active = true;
};

// Serialize - pass the output target to the constructor
person p{"Alice", 30, true};
std::string out;
json::serializer ser{out};
ser.dump(p);
// out -> {"name":"Alice","age":30,"active":true}

// Deserialize - pass the input to the constructor, call load<T>()
auto p2 = json::deserializer{std::string{R"({"name":"Bob","age":25,"active":false})"}}.load<person>();
// p2.name == "Bob", p2.age == 25
```

---

## JSON serializer

`json::serializer<OutputIt>` holds an output iterator and writes JSON text.

Deduction guides are provided for `std::string`, `std::ostringstream`, and
`std::ofstream`:

```cpp
std::string          out;   json::serializer ser{out};   // -> back_insert_iterator
std::ostringstream   oss;   json::serializer ser{oss};   // -> ostreambuf_iterator
std::ofstream        file;  json::serializer ser{file};  // -> ostreambuf_iterator
```

Use `dump()` to serialize, or call the `serde::serialize` CPO directly:

```cpp
std::string      out;
json::serializer ser{out};

ser.dump(std::vector<int>{1, 2, 3});          // [1,2,3]
ser.dump(std::map<std::string,int>{{"a",1}}); // {"a":1}
```

---

## JSON deserializer

`json::deserializer<It>` holds an input iterator pair and parses JSON.

```cpp
std::string        str{"..."}; json::deserializer de{str};       // string iterator
std::istringstream iss{"..."}; json::deserializer de{iss};       // istreambuf_iterator
json::deserializer de{begin_it, end_it};                         // explicit iterator pair
```

Use `load<T>()` to parse into a concrete type:

```cpp
// Into a json::value (poly::var)
auto v = json::deserializer{std::string{R"([1,"two",null])"}}.load();
v.is_array();   // true

// Into a concrete struct (reflection-driven field matching)
struct config { std::string host; int port = 80; };
auto cfg = json::deserializer{std::string{R"({"host":"localhost","port":8080})"}}.load<config>();
```

---

## BSON serializer

`bson::serializer<OutputIt>` holds a byte-compatible output iterator.

Deduction guides are provided for `std::vector<std::byte>`, `std::ofstream`,
and other byte / char streams:

```cpp
std::vector<std::byte> buf;   bson::serializer ser{buf};   // back_insert_iterator
std::ofstream          file;  bson::serializer ser{file};  // ostreambuf_iterator
```

```cpp
struct point { int x; int y; };

std::vector<std::byte> buf;
bson::serializer ser{buf};
ser.dump(point{1, 2});
```

---

## BSON deserializer

`bson::deserializer<It>` holds a byte-compatible input iterator pair.

Deduction guides are provided for `std::vector<std::byte>`, `std::ifstream`,
and other input ranges:

```cpp
std::vector<std::byte> buf{...};  bson::deserializer de{buf};          // range constructor
std::ifstream          file;    bson::deserializer de{file};          // istreambuf_iterator
bson::deserializer de{buf.begin(), buf.end()};                        // explicit pair
```

```cpp
auto p = bson::deserializer{buf}.load<point>();
// p.x == 1, p.y == 2
```

BSON is a document format, so scalars are transparently wrapped in a
`{"value": ...}` envelope.  Maps and aggregates serialize as top-level BSON
documents.

---

## File round-trip example

```cpp
import reflex.serde.bson;

struct S { int n; std::string label; };

// Write
S original{42, "hello"};
{
  std::ofstream    file{"data.bson", std::ios::binary};
  bson::serializer ser{file};
  ser.dump(original);
}

// Read
{
  std::ifstream      file{"data.bson", std::ios::binary};
  bson::deserializer de{file};
  auto loaded = de.load<S>();
  // loaded.n == 42, loaded.label == "hello"
}
```

---

## Field renaming with annotations

Use `serde::rename` or naming-convention annotations to control serialized keys:

```cpp
import reflex.serde;

struct my_struct
{
  [[= serde::rename{"first_name"}]] std::string firstName;
  int age = 0;
};
// JSON: {"first_name":"...","age":0}

struct [[= serde::naming::camel_case]] api_response
{
  int    status_code = 200;
  [[= serde::naming::kebab_case]] std::string content_type;
};
// JSON: {"statusCode":200,"content-type":"..."}
```

---

## `poly` serde

`reflex.serde` provides direct support for `reflex::poly::var`:

```cpp
import reflex.serde.json;
using value = reflex::poly::var<bool, std::int64_t, double, std::string>;

auto v = json::deserializer{std::string{R"({"x":1,"y":2})"}}.load<value>();
v.is_object(); // true
```

---

## Extending with `tag_invoke`

Both backends use the `serde::serialize` / `serde::deserialize` CPOs
dispatched via `tag_invoke`.  To add support for a custom type:

```cpp
namespace reflex::serde::json
{
  // Serialize MyType as a JSON string
  template <typename OutputIt>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, MyType const& v)
  {
    return tag_invoke(tag_t<serde::serialize>{}, ser, v.to_string());
  }

  // Deserialize MyType from a JSON string
  template <typename It>
  MyType tag_invoke(tag_t<serde::deserialize>, deserializer<It>& de, std::type_identity<MyType>)
  {
    return MyType::from_string(de.load<std::string>());
  }
}
```

The same pattern applies to `reflex::serde::bson`.

---

> See [tests](tests) for more examples.
