# reflex.serde

> **`ser`ialization / `de`serialization for C++26.**

`reflex.serde` lets you serialize and deserialize C++ aggregates, standard
containers, and `reflex::poly::var` values without writing any boilerplate -
field names, nesting, and type dispatch are all derived automatically via
C++26 static reflection.

Ships with **JSON** (`reflex.serde.json`), **BSON** (`reflex.serde.bson`),
**CSV** (`reflex.serde.csv`), **XML** (`reflex.serde.xml`), and **YAML**
(`reflex.serde.yaml`) backends.

---

## Modules

| Module | Purpose |
|---|---|
| `reflex.serde` | Core concepts, annotations, `object_visit` |
| `reflex.serde.json` | JSON serializer + deserializer |
| `reflex.serde.bson` | BSON serializer + deserializer |
| `reflex.serde.csv` | CSV serializer + deserializer (flat aggregates) |
| `reflex.serde.xml` | XML serializer + deserializer |
| `reflex.serde.yaml` | YAML serializer + deserializer |

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

## XML attributes

Annotate a member with `xml::attribute` to serialize it as an attribute on the
element's open tag instead of a child element. Attribute members must be scalar
(a text type or an optional of one), `serde::rename` and naming annotations
apply, and an empty optional attribute is omitted:

```cpp
import reflex.serde.xml;

struct Price
{
  [[= xml::attribute]] std::string currency;
  double                           amount;
};
// XML: <Price currency="USD"><amount>42.5</amount></Price>
```

On read, attributes match members by serialized name; unknown attributes are
ignored.

`xml::text` puts a member in the element's text content instead of a child
element (attributes plus text, no child elements). An absent optional text
member self-closes the element:

```cpp
struct Measure
{
  [[= xml::attribute]] std::string unit;
  [[= xml::text]]      double       value;
};
// XML: <Measure unit="kg">42.5</Measure>
```

`xml::raw_content` captures an element's inner XML verbatim (escape hatch for
mixed content). It is read and written unparsed, so the caller owns
well-formedness:

```cpp
struct Doc
{
  [[= xml::attribute]]   std::string lang;
  [[= xml::raw_content]] std::string body; // inner XML, byte-exact
};
```

`xml::text` and `xml::raw_content` are each limited to one member, are mutually
exclusive, and cannot coexist with child-element members.

`xml::cdata` writes a `str_c` member inside a CDATA section instead of
entity-escaping it (a `]]>` in the payload is split across two sections). CDATA
is read transparently, so the annotation only affects serialization:

```cpp
struct Script
{
  std::string             name;
  [[= xml::cdata]] std::string code;
};
// XML: <Script><name>s</name><code><![CDATA[if (a < b) x;]]></code></Script>
```

On read, any element's text content accepts CDATA sections, plain text, and
entity references interchangeably (they concatenate).

## XML namespaces

Prefix-based, no URI resolution. Reading is tolerant by default: a prefixed name
(`<x:name>`) matches a member by its local name when no exact qualified match
exists (an exact match wins), and `xmlns` / `xmlns:*` declarations are ignored.

Writing is opt-in. Annotate an aggregate with `xml::ns{prefix, uri}`: its element
and child-element tags get the prefix and the open tag gets an `xmlns:prefix`
declaration. A member-level `xml::ns` overrides the prefix for that field:

```cpp
struct[[= xml::ns{"x", "urn:example:e"}]] Env
{
  int a;
};
// XML: <x:Env xmlns:x="urn:example:e"><x:a>1</x:a></x:Env>
```

---

## YAML

Block style out, block and flow in. Any valid JSON document is valid input,
since YAML is a JSON superset.

```cpp
import reflex.serde.yaml;

struct Server
{
  std::string      host;
  int              port;
  std::vector<int> workers;
};

Server s{"localhost", 8080, {1, 2}};
std::string out;
yaml::serializer ser{out};
ser.dump(s);
// host: localhost
// port: 8080
// workers:
//   - 1
//   - 2

auto back = yaml::deserializer{out}.load<Server>();
```

Two-space indent, fixed. Output carries no trailing newline, so a round trip is
byte-exact. An empty collection is written `[]` or `{}`, which is the only
spelling YAML has for one.

### Quoting

A scalar is written plain when that reads back unchanged, single-quoted when it
can be, and double-quoted only when a control character leaves no choice:

```cpp
dump("1.2.3-rc1"s);  // 1.2.3-rc1   plain
dump("42"s);         // '42'        would resolve as a number
dump("null"s);       // 'null'      would resolve as null
dump("yes"s);        // 'yes'       a boolean to a YAML 1.1 reader
dump("a: b"s);       // 'a: b'      a colon-space cannot appear plain
dump("a\nb"s);       // "a\nb"      needs an escape
```

The YAML 1.1 booleans (`yes`/`no`/`on`/`off`/`y`/`n`) are quoted deliberately.
YAML 1.2 reads them as strings, so this library would round-trip them either
way, but a 1.1 reader would not, and a document that means two things to two
readers is a bug.

Symmetrically, `load<bool>()` **refuses** them rather than guessing - asked for
a `bool`, the intent is unambiguous and a 1.1 spelling is worth an error. Asked
what a value *is* (`load<yaml::value>()`), 1.2 answers "a string".

### Values

`yaml::value` is `poly::var<double, bool, std::string>` - the same alternatives
as `json::value`, deliberately, so a document keeps its shape when converted
between the two. Numbers are `double`, so an integer past 2^53 loses precision,
exactly as through the JSON backend.

Quoting defeats resolution, which is what makes a round trip work: `42` loads as
a number and `'42'` as a string.

### Supported

| | |
|---|---|
| dialect | YAML 1.2 core schema |
| scalars | plain, single-quoted, double-quoted, literal `\|`, folded `>`, with `-`/`+` chomping and an explicit indent digit |
| collections | block mappings and sequences, flow `[...]` and `{...}` on input |
| sequence under a key | both the indented and the flush spelling are accepted; the indented one is written |
| multi-line plain scalars | a plain value folds across the lines it continues onto |
| comments | `#` after whitespace or at the start of a line |
| line breaks | `\n`, `\r\n`, and a lone `\r` |
| inputs | any contiguous range, `serde::mmap_input_stream`, or an `std::istream` |

### Multi-line plain scalars

A plain value continues onto any line indented deeper than its key, folding one
line break to a space and each blank line to a newline:

```yaml
summary: a description long enough
  that it runs onto a second line
note: first paragraph

  second paragraph
```

gives `"a description long enough that it runs onto a second line"` and
`"first paragraph\nsecond paragraph"`.

A continuation ends at a line that is not indented deeper, at a comment, and at
a line that looks like a mapping of its own - `key: a` over `  b: c` is
ambiguous with a nested block, so it is an error, as it is in real YAML. A `- `
or `? ` on a continuation line is **not** ambiguous and folds into the text.

Quoted scalars are read on one line only, and a plain scalar inside a flow
collection does not continue. Both fold in real YAML; use `|` or `>` here.

### Not supported

Each of these throws with a message naming the feature, rather than being
mis-parsed:

- anchors `&a` and aliases `*a`
- explicit tags (`!!str`, `!foo`)
- merge keys (`<<:`)
- more than one document in a stream (a single leading `---` is accepted)
- `%YAML` / `%TAG` directives
- a tab used to indent a node
- a multi-line **quoted** scalar, and a multi-line plain scalar inside a flow
  collection

Explicit-key syntax (`? key`) is **not** in that list, and not because it works:
`? a` is currently read as the plain scalar `"? a"` rather than refused. That is
a gap, not a feature.

Over-indentation inside a block is an error rather than being tolerated, which
is stricter than most YAML readers. Since a plain value does continue onto
deeper lines, what remains an error is the ambiguous case: a deeper line that
looks like a mapping of its own.

A `std::string_view` member cannot be a YAML destination. Unlike the JSON and
XML backends this one decodes a scalar rather than pointing at the input, so
there is no borrowed read; the compiler says so and names `std::string`.

Parsing is slower than the JSON backend on the same data. YAML is indentation
sensitive and cannot be lexed with a single byte of lookahead, so most of the
parse runs a byte at a time. For a config file read once at start-up this does
not matter; for bulk interchange, use JSON.

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

Every backend uses the `serde::serialize` / `serde::deserialize` CPOs
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
