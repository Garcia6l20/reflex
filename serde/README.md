# reflex.serde

> **`ser`ialization / `de`serialization for C++26.**

`reflex.serde` lets you serialize and deserialize C++ aggregates, standard
containers, and `reflex::poly::var` values without writing any boilerplate -
field names, nesting, and type dispatch are all derived automatically via
C++26 static reflection.

Ships with **JSON** (`reflex.serde.json`), **BSON** (`reflex.serde.bson`),
**CSV** (`reflex.serde.csv`), **XML** (`reflex.serde.xml`), **YAML**
(`reflex.serde.yaml`) and **TOML** (`reflex.serde.toml`) backends.

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
| `reflex.serde.toml` | TOML serializer + deserializer |

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

## Omitting an empty field

`serde::omit_if_empty` leaves a field out of the output when it carries nothing: a
disengaged `std::optional`, an empty range, an empty string, or a `std::array` of
char whose first byte is NUL. It is opt-in per field, because emitting `null` or
`[]` is what most consumers expect.

```cpp
import reflex.serde;

struct config
{
  [[= serde::omit_if_empty{}]] std::optional<int> port;
  [[= serde::omit_if_empty{}]] std::vector<int>   ids;
  std::string                                     host;
};
// JSON, empty: {"host":""}
// JSON, filled: {"port":8080,"ids":[1],"host":"localhost"}
```

On a type it reaches every field, and a list of types narrows which ones. An entry
naming a template matches any instance of it, an entry naming a type matches that
type:

```cpp
struct [[= serde::omit_if_empty{^^std::optional, ^^std::string}]] request
{
  std::optional<int>       timeout;   // omitted when disengaged
  std::string              body;      // omitted when empty
  std::vector<std::string> headers;   // kept, the list excludes it
  [[= serde::no_omit]] std::string trace;  // kept, whatever the type-level annotation says
};
```

`serde::no_omit` cancels a type-level annotation on one field. It is also the way
past the two backends that refuse to omit, below.

The annotation is rejected, with a message, where it could not mean anything:

- on a field whose type can never be empty, such as an `int` or a
  `std::array<int, 3>`
- on a field its own type list excludes
- `serde::no_omit` on a type rather than on a field

### What each backend does with it

| backend | an annotated empty field |
|---|---|
| JSON | the key is absent. An all-omitted aggregate writes `{}` |
| BSON | the element is absent |
| YAML | the line is absent. An all-omitted mapping writes `{}` |
| TOML | the assignment, or the whole `[header]`, is absent |
| XML | the child element or the attribute is absent |
| CSV | rejected at compile time |

CSV refuses because the header fixes the column set: dropping a cell would shift
every later column, and an empty optional already writes an empty cell. XML
refuses on an `xml::text` or `xml::raw_content` field, which is the element's own
body rather than a key that can vanish. Both refusals name `serde::no_omit` as the
way out.

XML and TOML already drop some empty fields without any annotation, and that has
not changed. XML omits a disengaged optional and an empty sequence, TOML omits a
disengaged optional key and an empty array of tables. On those fields the
annotation asks for what already happens, and what it adds is the empty string and
the empty table.

Reading is unaffected. Every backend already leaves a field at its default when
the document does not mention it, so a document whose empty fields were omitted
round-trips to an equal value.

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

## TOML

TOML 1.1.0 in and out. A nested aggregate becomes a `[header]` section and a
sequence of aggregates becomes an array of tables, so the output reads like a
hand-written config file rather than one long inline table.

```cpp
import reflex.serde.toml;

struct Tls
{
  bool enabled;
  int  port;
};

struct Server
{
  std::string      host;
  std::vector<int> workers;
  Tls              tls;
};

Server s{"localhost", {1, 2}, {true, 8443}};
std::string out;
toml::serializer ser{out};
ser.dump(s);
// host = "localhost"
// workers = [1, 2]
// [tls]
// enabled = true
// port = 8443

auto back = toml::deserializer{out}.load<Server>();
```

Output carries no leading and no trailing newline and no blank line between
sections, so a round trip is byte-exact.

### Table layout

Every key at a table level is written **before** any `[subtable]` header at that
level. A key belongs to whichever header precedes it, so a member declared after
a nested struct still has to be written before it - the writer makes three passes
over the members rather than one:

```cpp
struct Probe
{
  std::string host;
  Tls         tls;   // declared before `port`
  int         port;
};
// host = "h"
// port = 8080       <- written first anyway
// [tls]
// enabled = true
// port = 8443
```

| Member | Written as |
| --- | --- |
| a scalar | `key = value` |
| `std::vector<int>` | `key = [1, 2, 3]`, on one line |
| an aggregate | `[key]` and then its body |
| `std::vector<Aggregate>` | `[[key]]` once per element |
| `std::map<std::string, T>` | `[key]` and then one entry per map key |
| `std::vector<std::vector<Aggregate>>` | inline, since an inline array has no line for a header |
| an empty `std::optional` | nothing, key included |
| `std::pair<K, V>` | a one-entry table |

A map member gets its own header before its entries, the way any other table
member does, and a map key that is not a bare key is quoted inside the path:

```cpp
struct MapDoc { std::map<std::string, Tls> m; };
MapDoc{{{"a", {true, 1}}, {"has space", {false, 2}}}};
// [m]
// [m.a]
// enabled = true
// port = 1
// [m."has space"]
// enabled = false
// port = 2
```

An empty aggregate still gets its header: `[child]` with nothing under it is a
distinct TOML value and dropping the header would lose it. An empty
`std::vector<Aggregate>` writes nothing at all, because `[[items]]` with no body
is one element rather than zero.

An empty `std::optional` drops its key. TOML has no null, so writing anything
would be a lie and omission is what a TOML reader expects. An inline table has a
key to omit too. An array element does not - `x = [1, , 3]` is not TOML - so a
null reaching an array element throws, with a message naming the format.

### Strings

A string is written as a basic string `"..."` unless a literal string `'...'`
saves an escape:

```cpp
dump(R"(C:\path\to)"s);      // 'C:\path\to'      beats "C:\\path\\to"
dump(R"(a "quoted" word)"s); // 'a "quoted" word'
dump("it's plain"s);         // "it's plain"      nothing to save
dump(R"(it's a \ mess)"s);   // "it's a \\ mess"  an apostrophe rules out literal
dump("C:\\a\tb"s);           // "C:\\a\tb"        a control byte rules it out too
```

A literal string may legally hold a raw tab, so "needs no escape" and "is legal
in a literal string" are not the same test: the literal form is refused for any
control character, tab included, rather than writing an invisible byte.

Control bytes use the 1.1 escape forms. `\0`, `\a` and `\v` have no TOML
spelling at all, unlike YAML:

```cpp
dump("\x1B"s);          // "\e"      not \u001b
dump("\x01"s);          // "\x01"    not \u0001
dump("\x7F"s);          // "\x7f"
dump("\b\t\n\f\r"s);    // "\b\t\n\f\r"
dump("\0\a\v"s);        // "\x00\x07\x0b"
```

All four string forms are read: basic, literal, multi-line basic `"""..."""` and
multi-line literal `'''...'''`. A trailing backslash in a multi-line basic string
deletes the line break and every space, tab and blank line after it. Neither
multi-line form is ever written.

A key is bare when it can be. A bare key is `[A-Za-z0-9_-]+` in 1.1.0 exactly as
in 1.0, so an identifier always is one and only a `serde::rename` produces
something else:

```cpp
struct RenameDoc
{
  int                                   plain;
  [[= serde::rename{"with space"}]] int spaced;
  [[= serde::rename{"my table"}]] Tls   t;
};
// plain = 1
// "with space" = 2
// ["my table"]
// ...
```

### Values

| TOML | C++ |
| --- | --- |
| Integer | any integral type. `toml::integer` is `std::int64_t` |
| Float | any floating-point type. `toml::number` is `double` |
| Boolean | `bool` |
| String | `std::string`, `reflex::heapless::string<N>`, `std::array<char, N>` |
| Array | any sequence |
| Table | an aggregate, a `std::map`, a `std::pair` |
| the four date-time types | `std::string`, verbatim |

Integer and Float are two types in TOML rather than two spellings of one, and
this backend keeps them apart on both sides. A `double` whose shortest form
carries no `.` and no exponent gains a `.0`, because a bare `1` is an Integer
wherever it appears:

```cpp
dump(1);                    // 1
dump(1.0);                  // 1.0
dump(1e30);                 // 1e+30
dump(std::int64_t{9007199254740993});  // 9007199254740993, exact
```

`toml::value` is `poly::var<std::int64_t, double, bool, std::string>`. Unlike
`json::value` and `yaml::value`, which collapse both numeric types into a
`double`, it carries a distinct Integer alternative, so an integer survives a
document round trip at full 64-bit width.

A whole document loaded as a `toml::value` writes back with the same table layout
a struct gets, header sections and arrays of tables included.

### Supported

| | |
|---|---|
| dialect | [TOML 1.1.0](https://toml.io/en/v1.1.0), read and written |
| strings | all four forms read, basic or literal written |
| escapes | `\b \t \n \f \r \e \" \\`, plus `\xHH`, `\uHHHH` and `\UHHHHHHHH` up to U+007F |
| integers | decimal with `_` separators, and `0x`, `0o`, `0b` |
| floats | `_` separators, exponents, `inf`, `-inf`, `nan` |
| keys | bare, basic-quoted, literal-quoted, dotted, and `[ a . b ]` with spaces around the dot |
| tables | `[a.b.c]` headers, dotted keys, inline `{ ... }` across lines with a trailing comma |
| arrays | `[ ... ]` across lines with a trailing comma, and `[[a.b]]` arrays of tables |
| comments | `#` to the end of the line |
| line breaks | `\n` and `\r\n` |
| inputs | any contiguous range, `serde::mmap_input_stream`, or an `std::istream` |

The reader rejects a document that defines the same thing twice, rather than
letting the last one win:

- a duplicate key in the same table, written directly or reached through a header
- a duplicate `[header]`, including `[a]` / `[a.b]` / `[a]`
- a `[header]` naming a path a previous assignment already made a value, whether
  that value was a scalar or an inline table
- a `[header]` reaching *through* a value: `b = { c = 1 }` then `[b.c.d]`
- a dotted key redefining a table a header defined, and a header redefining a
  table a dotted key created
- `[[x]]` on a path `[x]` already claimed, and `[x]` on a path `[[x]]` claimed
- `x = []` then `[[x]]`

### Not supported

**A 1.1 document is written, and a 1.0 parser reads it unless a string holds a
control character.** `\e` and `\xHH` are the only 1.1-only forms this backend
emits and both are reachable only from a control byte inside a string.
Everything else it writes parses under TOML 1.0. On the read side it accepts
three things 1.0 does not: `\e` and `\xHH`, a line break or trailing comma inside
an inline table, and a local time with the seconds left off.

**Date-times are strings.** The four date-time types are read verbatim into a
`std::string` and written back from one, with no `std::chrono` mapping. So
`d = 1979-05-27T07:32:00Z` comes back out as `d = "1979-05-27T07:32:00Z"` - a
quoted string, not a date-time. Code that wants a `std::chrono` type parses the
string itself.

**An escape above U+007F throws on read.** `\xHH`, `\uHHHH` and `\UHHHHHHHH` each
decode to one byte here, so anything above 0x7F would need a multi-byte UTF-8
encoding this decoder does not do. `\xFF` means U+00FF, which is two bytes of
UTF-8, so the limit bites from `\x80` up and not from `\u0100`. This is shared
with the JSON backend, same decoder. A literal multi-byte UTF-8 sequence in the
source passes through untouched on both sides, keys included. The serializer is
unaffected: it emits `\xHH` only for control bytes, all below U+0080.

**A key path is capped at 32 segments.** `serde::max_key_depth` is 32, and a
dotted key or a header with more segments throws. TOML has no such limit, so this
is the one place the reader refuses a document a TOML 1.1 parser accepts.

**A destination too small for the document is an error.** A third `[[items]]`
into a `std::array<Child, 2>`, or an over-long inline array, throws. The document
is valid and the destination cannot hold it.

**A `[header]` naming a member the destination does not have throws even with
nothing under it**, because a header materializes its table. That is stricter
than a TOML parser, which has no destination to be wrong about.

**Errors carry no line or column.** Every message names TOML and the offending
key path or header. No backend in this repo reports a position.

**Key order is not preserved.** A `std::map` destination and a `toml::value` are
both sorted by key, so a document round-tripped through either comes back
reordered. TOML does not require order to be kept.

**A `toml::value` member of an aggregate is written inline**, not as a header
section, because the member passes classify on the member type and a `poly::var`
is not a table type at compile time. A whole document loaded as a `toml::value`
is unaffected.

**A `std::string_view` member cannot be a TOML destination.** There is no
borrowed read: three of the four string forms are decoded rather than pointed at,
so the borrowable subset is a single-line literal string on a contiguous cursor,
which is too narrow to be worth a third string overload. The compiler says so and
names `std::string`.

**Conversion to and from the other backends' value types is not supported and is
not a goal.** `toml::value` models TOML. `json::value` and `yaml::value` collapse
Integer and Float into one `double`. `reflex-serde-convert` takes `toml` on
either side because it takes a format name on each side independently, but a
toml -> json -> toml trip turns `port = 8080` into `port = 8080.0` and
`9007199254740993` into `9007199254740992.0`. Nothing here maps one backend's
value type onto another's.

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
