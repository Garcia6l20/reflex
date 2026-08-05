# reflex.jinja

> **Jinja-style templating for C++26.**

`reflex.jinja` parses a template once into a tree of `std::string_view` slices
into its source, then renders it against a typed context. The context type is
built by reflection: bind a C++ aggregate, a `std::vector` or a `std::map` and
the members reachable from it become readable from template expressions, with
no glue code and no runtime type registration.

---

## Quick start

```cpp
import reflex.jinja;
using namespace reflex;
using namespace std::string_literals;

auto tmpl = jinja::parse("hello {{ world }}");

jinja::basic_context ctx;
ctx.set("world", "reflex"s);

auto out = jinja::render(tmpl, ctx);   // "hello reflex"
```

`parse()` is `constexpr` and keeps views into its argument: the source must
outlive the `template_`. `environment` (below) owns both and removes that
constraint.

### Binding aggregates

```cpp
struct row { int a; std::string b; };

std::vector<row> rows{{1, "one"s}, {2, "two"s}};

using namespace reflex::literals;
auto ctx = jinja::expr::context{"rows"_na = rows};   // _na: named argument

auto tmpl = jinja::parse("{% for r in rows %}{{ r.a }}:{{ r.b }} {% endfor %}");
jinja::render(tmpl, ctx);   // "1:one 2:two "
```

`context<Ts...>` scans the bound types at compile time - members of aggregates,
element types of sequences, mapped types of maps, the payload of
`std::optional` - and derives the `poly::var` alternative list from them.
Aggregates bind by reference, scalars and strings by value.

---

## Context

```cpp
template <typename... Ts> struct context;   // reflex::jinja::context
using basic_context = context<>;            // no compile-time bound types
```

| Member | Purpose |
|---|---|
| `set(name, value)` | Bind a global variable |
| `def(name, fn)` | Register a callable, `value_type(std::span<const value_type>) const` |
| `operator[](name)` | Read a variable, dotted paths included |
| `get<T>(name)` | `std::optional<T&>` on the bound object |
| `push_locals()` | RAII scope for local variables, used by `{% for %}` |
| `push_function(name, fn)` | RAII scope for a non-owning function, used by `super()` |
| `value_type` | The derived `poly::var<...>` |
| `object_type` / `array_type` | `obj<...>` / `arr<...>` of `value_type` |

```cpp
jinja::basic_context ctx;
using value = decltype(ctx)::value_type;

ctx.set("value", 42)
   .def("add", [](std::span<const value> args) -> value {
     return std::get<int>(args[0]) + std::get<int>(args[1]);
   });
```

An unknown variable evaluates to `null`. A call resolves against `funcs` first
and the [builtin table](#builtins) second, and an unknown name throws naming
itself.

---

## Tags

| Tag | Notes |
|---|---|
| `{{ expr }}` | Evaluates and formats, throws if the value is not formattable |
| `{# ... #}` | Comment, dropped at parse time |
| `{% if c %}` / `{% elif c %}` / `{% else %}` / `{% endif %}` | Truthiness follows `evaluate_bool` |
| `{% for x in seq %}` / `{% endfor %}` | Also `{% for k, v in map %}` |
| `{% set name = expr %}` | Binds `expr` under `name`, in the innermost scope |
| `{% include "name" %}` | Needs an `environment`, shares the caller's context |
| `{% extends "base" %}` | Needs an `environment`, must be the first meaningful tag |
| `{% block name %}` / `{% endblock %}` | `{% endblock name %}` is checked against the opening tag. `super()` is callable in the body |

A `{` that starts none of these is literal text.

### Loops

Inside a `{% for %}` body, `loop` is bound:

| Field | Type | Value |
|---|---|---|
| `loop.index0` | `int` | 0-based iteration |
| `loop.index` | `int` | 1-based iteration |
| `loop.first` | `bool` | First iteration |
| `loop.last` | `bool` | Last iteration |
| `loop.length` | `int` | Element count |
| `loop.parent` | `std::optional<loop_info&>` | Enclosing loop, null at the outermost level |

```jinja
{% for item in items %}{% if not loop.first %}, {% endif %}{{ loop.index }}: {{ item }}{% endfor %}
```

Over a map, one variable binds the key, two decompose into key and value. The
loop variables live in a scope popped at `{% endfor %}`.

### Whitespace control

`{%-` trims the whitespace of the text before the tag, `-%}` trims the
whitespace of the text after it. `{{ }}` does not take the modifier.

```jinja
{%- for item in items -%}
	{{ item }}
{%- endfor -%}
```
renders `bananaapplecherry` for `["banana", "apple", "cherry"]`.

---

## Expressions

Grammar, lowest precedence first:

```
expr      := pipe_expr
pipe_expr := or_expr   ( '|' ( identifier | call ) )*
or_expr   := and_expr  ( ('or'  | '||') and_expr )*
and_expr  := not_expr  ( ('and' | '&&') not_expr )*
not_expr  := ('not' | '!') not_expr | cmp_expr
cmp_expr  := add_expr  ( ('==' | '!=' | '<' | '<=' | '>' | '>=') add_expr )?
add_expr  := mul_expr  ( ('+' | '-') mul_expr )*
mul_expr  := unary     ( ('*' | '/' | '%') unary )*
unary     := '-' unary | primary
primary   := literal | identifier | call | '(' expr ')'
```

- Literals: `42`, `3.14`, `"text"`, `'text'`, `true`, `false`, `null` / `none` / `nil`.
- `+` concatenates strings, mixed int/double arithmetic promotes to `double`.
- Member access is dotted, `a.nested.b`, and subscript works on arrays and on
  bound sequences: `rows[1].b`.
- An empty `std::optional` reads as `null`.

```cpp
expr::evaluate("2 + 3 * 4");            // 14
expr::evaluate("user.age >= 18", ctx);  // bool
expr::evaluate_bool("s and not y", ctx);
```

### Filters

The pipe passes its left operand as the first argument of the call, so filters
are plain context functions:

```cpp
ctx.def("format", jinja_format<value>)
   .def("add", add)
   .def("mul", mul);
```
```jinja
{{ value | add(2) | mul(3) | format("{:>4}") }}
```

Pipes are usable anywhere an expression is: `{% for item in items | reverse() %}`.

A call resolves against the context's own functions first, the scoped functions
the renderer publishes second (`super()` is the only one), and the builtin table
below last, so a `def` of the same name always wins. That is what lets a
template override `format` or `sort` without renaming anything.

---

## Builtins

Around forty names are available with no registration. They are held in one
table per context value type, built on first use, and every one of them reports
its own name when it throws.

Every builtin takes its subject as argument 1, so each also works as a filter:
`length(s)` and `s | length` are the same call.

### Core

| Name | Signature | Notes |
|---|---|---|
| `length` / `count` | `length(x)` | Bytes of a string, elements of an array, entries of an object |
| `default` | `default(v, fallback)` | Tests `null`, not falsiness. `default(0, "x")` is `0` |
| `join` | `join(seq, sep = "")` | Each element rendered the way `{{ }}` renders it |
| `reverse` | `reverse(x)` | A string or an array. A string reverses bytes, not code points |
| `range` | `range(stop)`, `range(start, stop)`, `range(start, stop, step)` | Refuses above 1000000 elements. A zero step throws |
| `format` | `format(v, spec = "{}")` | `std::format` from inside a template |
| `tojson` | `tojson(v)` | The `serde::json` encoding of the value |

```jinja
{% for i in range(3) %}{{ i }}{% endfor %}   {# 012 #}
{{ 42 | format("0x{:x}") }}                  {# 0x2a #}
{{ missing | default("anon") }}
```

### Strings

| Name | Signature | Notes |
|---|---|---|
| `upper` | `upper(s)` | ASCII only |
| `lower` | `lower(s)` | ASCII only |
| `capitalize` | `capitalize(s)` | First byte upper, the rest lower |
| `trim` | `trim(s, chars = whitespace)` | Both ends |
| `replace` | `replace(s, from, to)` | Every occurrence. An empty `from` throws |
| `split` | `split(s)`, `split(s, sep)` | See below |
| `startswith` | `startswith(s, prefix)` | |
| `endswith` | `endswith(s, suffix)` | |
| `indent` | `indent(s, n, first = false)` | `n` spaces on every line after the first |
| `truncate` | `truncate(s, n, end = "...")` | `n` is the total length, the ellipsis included |

Case conversion is ASCII only, through the locale-free helpers in
`reflex/utils.hpp`. A UTF-8 multibyte sequence passes through unchanged.

`split(s)` splits on runs of whitespace and drops empty fields, so
`split("a b  c")` is three fields. `split(s, sep)` splits on the exact
separator and keeps them, so `split("a,,b", ",")` is three fields, the middle
one empty.

`indent` leaves the first line alone by default, because the filter is nearly
always used after text already sitting at the target column. A blank line is
not indented either, so no line ever gains trailing whitespace:
`indent("a\n\nb", 2)` is `"a\n\n  b"`.

### Case conversion

Each name has a second spelling matching `reflex::caseconv`. Both resolve to
one callable, so overriding one leaves the other on the builtin.

| Name | Alias | `"myField"` becomes |
|---|---|---|
| `snake(s)` | `to_snake_case(s)` | `my_field` |
| `camel(s)` | `to_camel_case(s)` | `myField` |
| `pascal(s)` | `to_pascal_case(s)` | `MyField` |
| `kebab(s)` | `to_kebab_case(s)` | `my-field` |
| `upper_snake(s)` | `to_upper_snake_case(s)` | `MY_FIELD` |

An alias reports the short name in its errors, since the two share one
callable: `to_snake_case()` throws `snake(): expects 1 argument, got 0`.

### Numbers

| Name | Signature | Notes |
|---|---|---|
| `abs` | `abs(x)` | Preserves the alternative, an int stays an int |
| `round` | `round(x, digits = 0)` | Always a double. Ties round away from zero |
| `int` | `int(x)` | Double truncates toward zero, string parses strictly, bool is 0 or 1 |
| `float` | `float(x)` | Symmetric with `int` |
| `string` | `string(x)` | The value rendered the way `{{ }}` renders it |
| `sum` | `sum(seq)` | Empty is `0`. Overflow is silent |
| `min` / `max` | `min(seq)` | A sequence, never varargs |

`int("12abc")` throws rather than returning `12`. `sum`, `min` and `max` take a
sequence because argument 1 of a pipe is the piped value, so `{{ a | max(b) }}`
could not read as `max([a, b])` from the same signature.

`/` on two integers is integer division. Cast first for an average:
`round(float(sum(xs)) / length(xs), 2)`.

### Sequences and objects

| Name | Signature | Notes |
|---|---|---|
| `first` | `first(x)` | A string or an array. Empty throws |
| `last` | `last(x)` | Same |
| `keys` | `keys(obj)` | Sorted by key |
| `values` | `values(obj)` | Same order as `keys` |
| `items` | `items(obj)` | One two-element array per entry |
| `unique` | `unique(seq)` | Order preserving, first occurrence wins |
| `sort` | `sort(seq)` | Numbers by value, strings lexicographically, a mix throws |
| `natsort` | `natsort(seq)` | Digit runs compare as numbers, the rest case-insensitively |

`first` throws on an empty sequence, so it does not compose with `default`:
`{{ xs | first | default("none") }}` throws before `default` ever runs.

`keys` and `values` come out sorted because `object_type` is a `std::map`.
Insertion order is not preserved and is not promised.

Read an `items` pair by subscript, or with `first` / `last` / `join`:

```jinja
{% for p in items(o) %}{{ p[0] }}={{ p[1] }};{% endfor %}
```

`natsort` puts `PA2` before `PA10`, which is what `sort` cannot do and what a
human reads as correct. The library therefore carries two orderings: `<` and
`compare` are numeric only and throw on anything else, while `sort` also orders
strings. Extending `<` was rejected because it would change the meaning of
`a < b` in every existing template.

### What is not in the table

`select`, `reject`, `map`, `groupby` and `batch` need a callable argument, and a
context function is `value_type(std::span<const value_type>)`, which cannot
express one. They are absent rather than approximated.

---

## Environment

`environment` owns the sources and the parsed trees, which is what keeps the
`string_view` slices of a `template_` valid, and resolves names through a
`loader`:

```cpp
using loader =
    std::copyable_function<std::optional<std::string>(std::string_view name) const>;
```

| Factory | Behavior |
|---|---|
| `filesystem_loader(root, ext = ".jinja")` | Reads `root/name[ext]`, refuses absolute names and any name climbing out of `root` through `..` |
| `map_loader(sources)` | Serves an in-memory `name -> source` map |

```cpp
jinja::environment env{jinja::filesystem_loader("templates")};

jinja::basic_context ctx;
ctx.set("x", 42);

env.render("page", ctx);                        // parses once, caches the tree
env.render_to(std::back_inserter(out), "page", ctx);
env.render_source("inline {{ x }}", ctx);       // takes a copy of the source
env.has("page");
env.get("page");                                // const template_&, throws when missing
```

A parse failure does not poison the cache, and a loader miss is only fatal at
`get`.

### Inheritance

`{% extends %}` walks the chain from the most-derived template to the root,
keeps the first override seen for each block name and renders the root. Blocks
nested inside `{% if %}` or `{% for %}` are overridable, block names are unique
per template, and a cycle throws.

```cpp
jinja::environment env{jinja::map_loader({
    {"base",  "[{% block body %}default{% endblock %}]"},
    {"child", "{% extends \"base\" %}{% block body %}hi {{ x }}{% endblock %}"},
})};

env.render("base", ctx);    // "[default]"
env.render("child", ctx);   // "[hi there]"
```

An `{% include %}` shares the context, including loop locals, but not the
includer's block overrides. Cyclic includes throw.

### `super()`

Inside an overriding block, `super()` returns the text of the definition it
overrides, one level down the chain per call:

```cpp
jinja::environment env{jinja::map_loader({
    {"c", "{% block body %}c{% endblock %}"},
    {"b", "{% extends 'c' %}{% block body %}b({{ super() }}){% endblock %}"},
    {"a", "{% extends 'b' %}{% block body %}a[{{ super() }}]{% endblock %}"},
})};

env.render("a", ctx);   // "a[b(c)]"
```

A level that does not override the block is skipped, and `super()` throws once
there is nothing left below, so it is an error in a base definition and outside
a block. It is an ordinary expression, so it composes:

```jinja
{{ super() | indent(4) }}
{% set parent = super() %}{{ parent }}{{ parent }}
```

The body it renders sees the context as it stands at the call, loop variables
included.

`super` is published as a `context::scoped_function_type`, a
`std::function_ref` valid for the block body only, so it costs no allocation and
a `def("super", ...)` still takes precedence. `context::push_function(name, fn)`
exposes the same mechanism, with the caller owning the callable.

A stored callable is a `std::copyable_function<... const>`, not a
`std::function`. The signature is const-qualified because `operator()` is, so a
callable that would mutate its own state through a const context fails to
compile rather than doing it silently.

---

## Limits

- No `{% macro %}`, `{% import %}`, `{% raw %}`, `{% filter %}` or `is` tests.
- No autoescaping.
- No `select`, `reject`, `map`, `groupby` or `batch`. Each needs a callable
  argument, which `value_type(std::span<const value_type>)` cannot express.
- Calls are positional. There is no keyword-argument form, so a filter wanting
  optional named parameters has to be split into fixed-arity names.
- `{% set %}` binds into the innermost scope: inside a `{% for %}` or
  `{% block %}` body the binding dies with the block, at top level it lands in
  the context globals. `{% if %}` opens no scope.
- `template_` is move-only, `clone()` copies the tree and re-indexes its blocks
  against the same source.
- Rendering without an environment throws on `{% include %}` and `{% extends %}`.

---

> See [tests](tests) for more examples.
