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

---

## Options, pretty output and comments

The JSON, YAML, TOML and XML serializers and deserializers take an options
struct as their last template argument. The defaults write the same bytes as a
serializer built without options. Pass the options to the constructor as a
`std::cw` tag and class template argument deduction fills in both template
arguments:

```cpp
std::string out;
json::serializer ser{out, std::cw<json::options{.indent = 2}>};
ser.dump(cfg);

json::deserializer de{text, std::cw<json::read_options{.comments = true}>};
auto cfg2 = de.load<settings>();

constexpr yaml::options wide{.indent = 4};
yaml::serializer{out, std::cw<wide>}.dump(cfg);
```

The tag goes last, after the string, stream, range or iterator pair the plain
constructor takes. Spelling the type out works too, and a constructor tag that
disagrees with the spelled options does not compile:

```cpp
json::serializer<std::back_insert_iterator<std::string>, json::options{.indent = 2}> ser{out};
```

Each struct holds `int`, `bool` and enum fields only, so it is usable as a
template argument.

| backend | `options` | `read_options` |
|---|---|---|
| JSON | `indent = 0`, `comments = false` | `comments = false`, `trailing_commas = false`, `unknown_keys = error` |
| YAML | `indent = 2`, `comments = false` | `comments = false`, `unknown_keys = error` |
| TOML | `comments = false` | `comments = false`, `unknown_keys = error` |
| XML | `indent = 0`, `comments = false` | `comments = false`, `unknown_keys = skip` |

BSON and CSV take no options.

### Pretty output

`indent` is the number of spaces per nesting level. JSON and XML write compact
output at 0 and indented output above it:

```cpp
settings value{"demo", {"localhost", 8080}};
json::serializer{out, std::cw<json::options{.indent = 2}>}.dump(value);
// {
//   "name": "demo",
//   "srv": {
//     "host": "localhost",
//     "port": 8080
//   }
// }

xml::serializer{out, std::cw<xml::options{.indent = 2}>}.dump(value);
// <settings>
//   <name>demo</name>
//   <srv>
//     <host>localhost</host>
//     <port>8080</port>
//   </srv>
// </settings>
```

XML indents only between elements. A leaf element, an `xml::text`,
`xml::raw_content` or `xml::cdata` body and any element with text content are
written exactly as in compact mode, since whitespace inside them is data. The
reader ignores whitespace between elements, so indented XML reads back equal.

YAML is always indented, 2 spaces by default. A sequence dash is padded to the
width, so every key of a sequence element sits on the same column:

```cpp
constexpr yaml::options wide{.indent = 4};
yaml::serializer{out, std::cw<wide>}.dump(std::vector<server>{{"a", 1}, {"b", 2}});
// -   host: a
//     port: 1
// -   host: b
//     port: 2
```

TOML indents nothing and has no `indent`.

### JSON5 comments and trailing commas

`json::read_options` accepts two JSON5 extensions, each behind its own flag:

```cpp
constexpr std::string_view text = R"({
  // where to listen
  "name": "demo",
  /* the listener */
  "srv": {"host": "localhost", "port": 8080,},
})";
json::deserializer de{text, std::cw<json::read_options{.comments = true, .trailing_commas = true}>};
auto s = de.load<settings>();
```

`comments` accepts `//` and `/* */` comments wherever whitespace may appear,
`/* */` in the middle of a line included. `trailing_commas` accepts one comma
after the last element of an array or object. Both apply to every reader,
`json::value` and `poly::var` included. A plain `load` skips the comments and
keeps none of them, see below to keep them. The writer never emits a trailing
comma, and writes comments only from a comment tree.

### Unknown keys

A key in the document that the destination aggregate has no member for is an
error by default in JSON, YAML and TOML, and skipped by default in XML.
`read_options::unknown_keys` selects either behaviour on any of the four:

```cpp
constexpr std::string_view text = R"({"name": "demo", "added_later": [1, 2], "srv": {"port": 1}})";

json::deserializer{text}.load<settings>();   // throws "Key not found in object"

json::deserializer de{text, std::cw<json::read_options{.unknown_keys = serde::unknown_keys::skip}>};
auto s = de.load<settings>();                // added_later is parsed and dropped

constexpr std::string_view doc = "<settings><name>demo</name><extra/></settings>";
xml::deserializer x{doc, std::cw<xml::read_options{.unknown_keys = serde::unknown_keys::error}>};
x.load<settings>();                          // throws "XML: no member matches the element 'extra'"
```

Under `skip` the value is still parsed, so it must be well-formed, and the
comments above the key are dropped with it. In TOML a skipped `[header]` drops
every key and sub-table under it, and the document must still be valid TOML.
Unknown XML attributes are always ignored. Keys of a `std::map` or a
`poly::var` destination are never unknown.

### Keeping comments across a read and a write

An application that reads a user's config file, changes a field and writes the
file back can keep the user's comments, blank lines and indentation. Read with
`load_commented<T>()` and write with `dump(value, comments)`:

```cpp
constexpr std::string_view input = R"(# Service settings

name: demo # shown in logs

# the listener
srv:
  host: localhost
  # change with care
  port: 8080 # behind the proxy
)";

yaml::deserializer de{input, std::cw<yaml::read_options{.comments = true}>};
auto [cfg, comments] = de.load_commented<settings>();
cfg.srv.port         = 9090;

std::string      out;
yaml::serializer ser{out, std::cw<yaml::options{.comments = true}>};
ser.dump(cfg, comments);
// out is the input with "port: 8080" changed to "port: 9090", every other byte equal
```

`load_commented<T>()` returns `serde::commented<T>`, a struct holding `value`
and `comments`. `T` must be default-constructible. The same two calls work on
all four backends and on every input a deserializer accepts.

The tree holds no format, so a config read from one format can be written to
another with its comments. Dumping the `cfg` and `comments` above:

```cpp
json::serializer{out, std::cw<json::options{.indent = 2, .comments = true}>}.dump(cfg, comments);
// // Service settings
//
// {
//   "name": "demo", // shown in logs
//
//   // the listener
//   "srv": {
//     "host": "localhost",
//     // change with care
//     "port": 9090 // behind the proxy
//   }
// }

toml::serializer{out, std::cw<toml::options{.comments = true}>}.dump(cfg, comments);
// # Service settings
//
// name = "demo" # shown in logs
//
// # the listener
// [srv]
// host = "localhost"
// # change with care
// port = 9090 # behind the proxy
```

#### The comment tree

`serde::comments_of<T>` is a struct generated from `T` by reflection. It has
one member per data member of `T`, with the same C++ name, whatever
`serde::rename` or a naming annotation says, so `comments.srv.port` mirrors
`cfg.srv.port`:

| model member | node |
|---|---|
| a nested aggregate | `comments_of<Member>` |
| `std::optional<U>` | the node of `U` |
| anything else: scalar, string, enum, sequence, map, `poly::var` | `serde::comment_block` |

```cpp
struct comment_block
{
  std::vector<std::string> leading;    // lines above the node, one entry per line
  std::string              trailing;   // the comment at the end of the node's line
  std::vector<std::string> footer;     // lines after an aggregate's last member
};
```

Entries hold the comment text without its marker and without the one space
after it. A blank line is an entry equal to `serde::blank_line`, the string
`"\n"`. No comment can hold a line break, so the two never collide, and a bare
`#` or `//` reads as the empty string. Every blank line is one entry.

An aggregate node holds its own block in a hidden member. Reach it with
`serde::comment(node)`, which also accepts a `comment_block` and returns it, so
it is the one spelling for every node:

```cpp
serde::comments_of<settings> c;
serde::comment(c).leading     = {"Service settings", std::string{serde::blank_line}};
c.name.trailing               = "shown in logs";
serde::comment(c.srv).leading = {"the listener"};
c.srv.port.leading            = {"change with care"};

yaml::serializer{out, std::cw<yaml::options{.comments = true}>}.dump(value, c);
// # Service settings
//
// name: demo # shown in logs
// # the listener
// srv:
//   host: localhost
//   # change with care
//   port: 8080
```

On the root, `leading` is the document header and `footer` the document footer.
In JSON and XML the header is every line before the opening `{` or the root's
start tag. In YAML and TOML it runs up to and including the last blank line
before the first key, and with no blank line there every comment above the
first key is that key's `leading`.

`serde::layout_of(root)` returns the layout measured on read:

```cpp
struct layout
{
  std::uint8_t indent        = 0;     // detected width, 0 when nothing was detected
  char         indent_char   = ' ';   // ' ' or '\t'
  bool         final_newline = true;  // whether the input ended with a line break
};
```

`dump(value, comments)` indents by `layout.indent` when it is not 0 and by
`options.indent` otherwise, so a file the user re-indented to 4 spaces stays at
4 spaces. It ends the output with a line break when `final_newline` is set,
which a plain `dump(value)` never does. A plain `dump(value)` on the same
serializer keeps `options.indent`. The XML commented dump indents even on a
serializer whose `indent` is 0 when the layout says so. TOML has no
indentation, its layout indent stays 0.

Trees compare with `==` and `!=`, member by member, layouts included:

```cpp
yaml::deserializer again{out, std::cw<yaml::read_options{.comments = true}>};
assert(again.load_commented<settings>().comments == comments);
```

#### Where a comment attaches

| format | `leading` | `trailing` | aggregate `footer` |
|---|---|---|---|
| JSON | `//` lines above the key | a comment after the value on the same line, before or after the comma | lines before the closing `}` |
| YAML | `#` lines above the key | `# x` after the value, or after `key:` for a nested block | lines after the last key, at or deeper than the block's column |
| TOML | `#` lines above the key or `[header]` | `# x` on the key's or the header's line | written after the table's whole body, see the limits |
| XML | `<!-- -->` lines above the element | after the close tag on the same line, or after the start tag of an element with children | lines before the close tag |

The JSON header is written before the opening `{` and the root footer before
the closing `}`.

#### Compile-time errors

Each of these is a `static_assert` naming the problem:

```cpp
json::serializer s1{out, std::cw<json::options{.comments = true}>};
// json::options: comments need indent > 0. A // comment runs to the end of its line, and
// indent 0 writes the whole document on one line

yaml::serializer s2{out, std::cw<yaml::options{.indent = 1}>};
// yaml::options::indent must be at least 2, the width of a sequence dash and its space

json::deserializer d1{text};
d1.load_commented<settings>();
// load_commented needs a deserializer whose read options set .comments = true, a grammar
// without comments has none to capture

json::serializer s3{out, std::cw<json::options{.indent = 2}>};
s3.dump(value, comments);
// dump(value, comments) needs a serializer whose options set .comments = true, a serializer
// that writes no comments would drop the tree silently
```

A model member named `_self` is rejected when `comments_of` first names the
model, since the hidden block uses that name. A model that never meets
`comments_of` may use it.

At run time, every writer throws on a comment holding a line break. XML also
throws on `--`, and TOML on a control character other than tab.

#### Known limits

The commented dump reproduces a file byte for byte when the file is written the
way the serializer writes it. Anything else is normalized on the first save:

- Key order follows the model. TOML also writes plain keys before tables and
  arrays of tables.
- A field absent from the file is written with its default value.
- Value spelling is the writer's: YAML quote and block-scalar style, TOML
  number bases and string forms, JSON number spelling such as `1.0`, XML
  self-closing tags and attribute quotes.
- One space separates a value from its trailing comment.
- One indent width and one indent character are detected. Mixed indentation is
  not kept.
- Sequence and map elements carry no comments yet. The member holding the
  sequence or map keeps its own comments, every comment inside it is dropped
  on read. The same holds inside `poly::var`, the `*_value` DOM types and
  inline values such as a TOML inline table.
- A member of a type with a user `tag_invoke` override keeps its own leading
  and trailing comments. Nothing inside it is captured or written.
- A member that writes no key drops its comments: an empty optional in TOML
  and XML, an empty sequence in XML.
- A key skipped under `unknown_keys::skip` loses its comments and disappears
  on write-back.

JSON:

- `/* */` comments are written back as `//` lines, one per line of the block.
- Trailing commas are accepted on read and never written.
- An array is written one element per line, an inline `[1, 2]` is not kept.

YAML:

- Comments inside flow collections and block scalar bodies are not captured.
- A comment on the root's own line has no place in the tree.
- A comment between flush sequence entries, at the key's column, becomes the
  next key's leading comment, or is dropped when a later entry of that
  sequence has a comment of its own.
- A comment indented deeper than its neighbours after a scalar value is
  dropped.
- The footer of an aggregate written as `{}` reads back as the next key's
  leading comments.
- Lines before a `---` marker are dropped.
- A sequence or scalar root puts every comment, header included, into its
  footer.

TOML:

- A table's footer is written after its whole body. Read back, it becomes the
  next header's leading comments, or the root footer after the last table.
- A blank line inside the first key's leading comments moves into the document
  header.
- Arrays of tables carry no comments, neither the member's own nor any
  element's.

XML:

- No XML declaration is written, a `<?xml ...?>` prolog is dropped.
- Comments after the root's close tag are written before it, inside the root.
- Compact XML does not keep comment positions. On one line, a comment before an
  element reads as the trailing comment of the one before it. Compact output
  writes no blank lines.
- A repeated element keeps the leading comments of its first occurrence and the
  trailing comment of its last.
- Comments inside a text run are dropped.
- A scalar root writes no final line break.

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

Two-space indent by default, `yaml::options::indent` changes it, see
[Pretty output](#pretty-output). Output carries no trailing newline, so a round
trip is byte-exact. An empty collection is written `[]` or `{}`, which is the only
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
than a TOML parser, which has no destination to be wrong about. Under
`unknown_keys::skip` the header and everything under it are skipped instead, see
[Unknown keys](#unknown-keys).

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
  template <typename OutputIt, auto Opts>
  OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt, Opts>& ser, MyType const& v)
  {
    return tag_invoke(tag_t<serde::serialize>{}, ser, v.to_string());
  }

  // Deserialize MyType from a JSON string
  template <typename It, auto Opts>
  MyType tag_invoke(
      tag_t<serde::deserialize>, deserializer<It, Opts>& de, std::type_identity<MyType>)
  {
    return MyType::from_string(de.template load<std::string>());
  }
}
```

The JSON, YAML, TOML and XML serializers and deserializers take an options
struct as their second template argument, defaulted. Spell an override with
`auto Opts` as above so it binds every option set. A nested `serialize` or
`load` call inside it then runs with the same options. `de.template load<T>()`
needs the `template` keyword, since `de` has a dependent type. An override written as
`serializer<OutputIt>&` binds the default options only, and under any other
set the backend's own overload for that type is used instead, silently.

The same pattern applies to `reflex::serde::bson`, whose serializer and
deserializer have no options argument: there the override takes
`serializer<OutputIt>&` and `deserializer<It>&`.

---

> See [tests](tests) for more examples.
