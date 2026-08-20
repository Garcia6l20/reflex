# reflex.qt

> **Qt metaobjects from C++26 reflection.** No `Q_OBJECT`, no `Q_GADGET`, no moc.

`reflex.qt` builds a class's `QMetaObject` at compile time from reflection over its
members. Derive from `reflex::qt::object<T>`, annotate what Qt should see, and stock Qt
treats the class as if moc had processed it: `connect`, `qobject_cast`,
`QMetaObject::invokeMethod`, `QVariant`, `QMetaProperty` and queued connections all work.

The metaobject it produces reads back identical to real moc's, field for field: class name,
class infos, method signatures and kinds, clone flags, constness, return and parameter
metatypes, parameter names for slots and invocables, property flags and notify indices, and
every enumerator. What differs is listed under [Differences from moc](#differences-from-moc).

QML works the same way: annotate a class with `[[= qt::qml{}]]`, name it in a module body,
and `qmltyperegistrar` registers it from a metatypes document reflection wrote. See
[Metatypes and QML](#metatypes-and-qml).

Header-only. `#include <reflex/qt.hpp>` and link `Qt6Core`. The `reflex/qt/moc/` headers
also need `reflex.serde` on the include path, and `reflex/qt/qml.hpp` needs `Qt6Qml`.

---

## Quick start

```cpp
#include <reflex/qt.hpp>

#include <QtCore/QMetaObject>
#include <QtCore/QObject>

namespace qt = reflex::qt;

struct counter : qt::object<counter>
{
  signal<int> changed{this};

  [[= qt::slot]] void bump()
  {
    setProperty<"value">(value + 1);
    changed(value);
  }

  [[= qt::prop{}]] int value = 0;
};
```

```cpp
counter c;
int     seen = 0;

QObject::connect(&c, &counter::changed, [&seen](int n) { seen = n; });
QMetaObject::invokeMethod(&c, "bump");
// c.value == 1, seen == 1
```

`counter` publishes one signal, one notify signal `valueChanged()`, one slot and one
property, in the method table order moc would use.

Every annotation lives in namespace `reflex::qt`. The snippets here open with
`namespace qt = reflex::qt;` and write `qt::prop{}`. A file-scope `using namespace
reflex::qt;` gives the bare `prop{}` instead. `signal` is the one name that comes from the
base rather than the namespace, because a signal has to know the class that emits it.

---

## Objects

`reflex::qt::object<Super, ParentT = QObject>` is a CRTP base. `Super` is the class being
declared, `ParentT` is its Qt base. Any `QObject` works as `ParentT`, a moc'ed one
included, so a reflex class can derive from `QWidget` and a reflex class can derive from
another reflex class.

```cpp
struct base_widget : qt::object<base_widget>
{
  [[= qt::prop{}]] int level = 0;
};

struct derived_widget : qt::object<derived_widget, base_widget>
{
  [[= qt::prop{}]] int depth = 0;
};
```

Method and property offsets come out the way moc computes them, so
`QMetaObject::superClass()` chains and an inherited property is written and notified
through the class that declares it.

### Signals

A signal is a data member of type `signal<Args...>`, constructed with `this`. Calling it
emits.

```cpp
struct emitter : qt::object<emitter>
{
  signal<>                        ping{this};
  signal<int, qt::defaulted<int>> pair{this, 42};

  [[= qt::slot]] void onPair(int a, int b)
  {
    sum = a + b;
  }

  int sum = 0;
};
```

```cpp
emitter e;
QObject::connect(&e, &emitter::pair, &e, &emitter::onPair);

e.pair(1, 2);   // e.sum == 3
e.pair(1);      // e.sum == 43, the default fills the missing argument
```

`qt::defaulted<T>` marks an argument that may be omitted, and its value is given at
construction. Qt publishes one method table entry per reachable arity, so `pair` appears as
both `pair(int,int)` and `pair(int)` and a `SIGNAL(pair(int))` connect delivers once.
Every `defaulted<T>` has to be trailing, the way a C++ default argument does; one followed
by a plain argument is a compile error naming the argument that follows it.

The count has to come from the signal's *type*. The metaobject emits the clone entries
while it reflects over the class, before any object exists, and the defaults are
constructor arguments that never appear in the member's type. That is why the marker
cannot be inferred away.

### Slots and invocables

`[[= qt::slot]]` publishes a member function as a slot, `[[= qt::invocable]]` as a `Q_INVOKABLE`
method. Both accept default arguments and both keep their parameter names.

```cpp
struct service : qt::object<service>
{
  [[= qt::slot]] void reset()
  {
    calls = 0;
  }

  [[= qt::invocable]] int twice(int n) const
  {
    return 2 * n;
  }

  int calls = 0;
};
```

```cpp
service s;
int     doubled = 0;

QMetaObject::invokeMethod(&s, "twice", Q_RETURN_ARG(int, doubled), Q_ARG(int, 21));
// doubled == 42
```

### Properties

`[[= qt::prop{}]]` on a data member publishes it as a `Q_PROPERTY`: readable, writable, and
notifying through a generated `<name>Changed` signal. The moc declaration it matches is
`Q_PROPERTY(int volume MEMBER volume NOTIFY volumeChanged)` plus the signal, not
`MEMBER` alone: moc generates no notify signal for a `MEMBER` property, so a bare
`Q_PROPERTY(int plainint MEMBER plainint_)` leaves the method table empty and
`hasNotifySignal()` false. `prop{.notify = false}` is what reads back like that one.

```cpp
struct settings : qt::object<settings>
{
  [[= qt::prop{}]] int                                volume = 0;
  [[= qt::prop{.write = false}]] int                  peak   = 0;
  [[= qt::prop{.notify = false}]] int                 cursor = 0;
  [[= qt::prop{.constant = true}]] int                limit  = 100;
  [[= qt::prop{.final = true, .required = true}]] int rate   = 44100;
};
```

| Field | Default | Effect when cleared or set |
|---|---|---|
| `.read` | `true` | clearing it drops `Readable` and the `ReadProperty` metacall |
| `.write` | `true` | clearing it drops `Writable`, a write through `QMetaProperty` fails |
| `.notify` | `true` | clearing it drops the `<name>Changed()` signal from the method table |
| `.constant` | `false` | setting it adds `Constant` and implies neither writable nor notifying |
| `.final` | `false` | setting it adds `Final` |
| `.required` | `false` | setting it adds `Required` |

`.constant` next to `WRITE` or `NOTIFY` is what moc rejects, so it wins over both here
rather than being diagnosed. The other flags default to `true` and an explicit `true`
cannot be told apart from the default.

Reading and writing works two ways. The typed form takes the name as a template argument
and is checked at compile time, the `QVariant` form is `QObject`'s own.

```cpp
settings s;

s.setProperty<"volume">(7);
const int typed = s.property<"volume">();       // 7, an int

s.setProperty("volume", 9);                     // QObject's, through QVariant
const QVariant boxed = s.property("volume");    // QVariant(int, 9)
```

A write of the value the property already holds notifies nothing, as moc's own
`setProperty` does. A property whose type is not equality-comparable is allowed and always
notifies.

### Accessors

A property backed by a data member can route its read, its write and its change handling
through member functions. The annotation names the property by reflection, so a typo is a
compile error at the annotation rather than an accessor that silently never runs.

```cpp
struct scaled : qt::object<scaled>
{
  [[= qt::prop{}]] int raw = 0;

  [[= qt::getter<^^raw>]] int getRaw() const
  {
    return raw * 2;
  }

  [[= qt::setter<^^raw>]] void setRaw(int value)
  {
    raw = value / 2;
  }

  [[= qt::listener<^^raw>]] void onRawChanged()
  {
    ++changes;
  }

  int changes = 0;
};
```

- A getter takes no argument and returns the property's type.
- A setter takes exactly one argument of the property's type. It replaces the whole write,
  change detection included, so a property with a setter notifies on every write.
- A listener takes no argument, runs after the write and before the notify signal, and is
  rejected on a property declared `.notify = false`.
- One accessor of each kind per property. A second one is a compile error.

### Naming conventions

`[[= qt::naming::qt_style]]` on the class finds the accessors by the names Qt code
usually gives them, with no annotation on any of them.

```cpp
struct[[= qt::naming::qt_style]] conventional : qt::object<conventional>
{
  [[= qt::prop{}]] int p1 = 0;

  int getP1() const
  {
    return p1 * 3;
  }

  void setP1(int value)
  {
    p1 = value / 3;
  }

  void onP1Changed()
  {
    ++changes;
  }

  int changes = 0;
};
```

An annotated accessor still wins over a conventionally named one. The listener is
`onP1Changed` and not `p1Changed`, because `p1Changed` is the name the metaobject already
publishes for the property's notify signal.

### Notify signals

A property gets a `<name>Changed()` signal unless it says `.notify = false` or
`.constant = true`. The connect target is `&Super::propertyChanged<"name">`.

```cpp
counter c;
int     notifications = 0;

QObject::connect(&c, &counter::propertyChanged<"value">, [&notifications] { ++notifications; });
c.setProperty<"value">(3);
// notifications == 1
```

Naming a property that publishes no notify signal is a compile error rather than a connect
target that never fires.

### Timers

A `qt::timer<^^handler>` data member declares a timer driving the member function `handler`.
The storage is one `int` and lives in the class that wants the timer, so a class with no
timer pays nothing.

```cpp
struct poller : qt::object<poller>
{
  void tick()
  {
    ++ticks;
  }

  qt::timer<^^tick> tick_timer;

  int ticks = 0;
};
```

```cpp
poller    p;
const int id = p.startTimer<^^poller::tick>(50);   // 0 if one is already running
p.tick_timer.isActive();                           // true
p.tick_timer.id();                                 // id
p.killTimer<^^poller::tick>();                     // true, false if none was running
```

`QObject::startTimer` and `QObject::killTimer` keep their untemplated overloads. A derived
class drives a timer its base declares, and a class that overrides `timerEvent` itself
takes over the dispatch.

#### One spelling of a handler per translation unit

`^^tick` written inside the class body and `^^poller::tick` written outside it reflect the
same member and compare equal, and each works on its own. GCC 16.2.1 emits the definition
twice when one translation unit instantiates the same template with both, and the assembler
rejects the duplicate symbol:

```
Error: symbol `_ZN6reflex2qt6objectI6poller7QObjectE10startTimerILDmfnNS2_4tickEvEEEii...'
       is already defined
```

The message names a mangled symbol and no line of your own code. reflex.qt cannot diagnose
it either: the collision happens after the front end and both spellings are correct C++.

So call `startTimer` and `killTimer` for one handler with one spelling per translation
unit. Inside the class body only `^^tick` is spellable, so a class that drives its own
timers keeps that spelling and publishes member functions for its callers.

```cpp
struct driver : qt::object<driver>
{
  int start()
  {
    return startTimer<^^tick>(50);
  }

  bool stop()
  {
    return killTimer<^^tick>();
  }

  void tick()
  {
    ++ticks;
  }

  qt::timer<^^tick> tick_timer;

  int ticks = 0;
};
```

A class that never names its own handler, `poller` above, leaves the calls to its callers,
which write `^^poller::tick`. The `qt::timer<^^tick>` member declaration instantiates
another template and takes no part in the rule.

Four declarations are rejected at compile time: a timer naming something that is not a
non-static member function, a timer naming a member function of neither its own class nor a
base, two timers naming one handler, and a `startTimer` naming a handler no timer member
declares.

---

## Gadgets

`reflex::qt::gadget<Super>` publishes a non-`QObject` value type, the way `Q_GADGET` does.
Properties, slots, invocables, class infos and enums all work, the way moc publishes them
on a `Q_GADGET`. Signals and timers do not: a gadget is not a `QObject`, so there is no
`QMetaObject::activate` to emit a signal and no `timerEvent` to dispatch a timer. Declaring
either is a compile error naming the member.

```cpp
struct point : qt::gadget<point>
{
  [[= qt::prop{}]] int x = 0;
  [[= qt::prop{}]] int y = 0;

  [[= qt::invocable]] int manhattan() const
  {
    return x + y;
  }
};
```

```cpp
point p;
point::staticMetaObject.property(0).writeOnGadget(&p, 3);
// p.x == 3

const QVariant boxed = QVariant::fromValue(p);
// boxed.metaType().flags() carries QMetaType::IsGadget
```

`QMetaType::fromType<point>()` is registered under the class name, `point` reports
`IsGadget` and `point*` reports `PointerToGadget`. `Q_DECLARE_METATYPE` is not needed.

---

## Enums and flags

Every nested enumeration is published, and so is every member alias of a `QFlags`
specialization over one of them. No annotation, no `Q_ENUM`, no `Q_FLAG`.

```cpp
struct styled : qt::object<styled>
{
  enum Color
  {
    Red,
    Green = 5,
    Blue
  };

  enum class Mode
  {
    Fast,
    Slow = 9
  };

  enum Option
  {
    NoOption = 0x0,
    First    = 0x1,
    Second   = 0x2
  };

  using Options = QFlags<Option>;

  [[= qt::prop{}]] Color   color = Red;
  [[= qt::prop{}]] Mode    mode  = Mode::Fast;
  [[= qt::prop{}]] Options options;
};
```

`QMetaEnum` reads back `isScoped`, `isFlag`, `is64Bit` and every key and value the way moc
would fill them. An enum-typed property carries `EnumOrFlag` and answers
`QMetaProperty::isEnumType()`.

What moc cannot do is publish any of this unmarked: it emits what `Q_ENUM` and `Q_FLAG`
name and nothing else. The two descriptors themselves it does reach, from
`Q_DECLARE_FLAGS(Options, Option)` plus both macros, which emits exactly the pair above.
What it never reaches is that pair from a plain `using Options = QFlags<Option>;`:
`Q_ENUM(Option)` plus `Q_FLAG(Options)` over an alias spelled that way collapses to the
single `Option` descriptor and drops the flag entry. Here both get their own, so
`QMetaEnum::valueToKey` on a bare `Option` value resolves. A `QFlags` alias whose argument is
declared in another class is skipped, because its enumerators belong to that class's
metaobject.

---

## Private members

moc publishes a class's private slots and private properties, so `reflex.qt` queries the
class with an unchecked access context and reaches members that are private to it.
Splicing one is still access-checked, and every such splice in the module is written inside
`reflex::qt::access<Super>`, so one friend declaration opens all of them.

```cpp
struct controller : qt::object<controller>
{
  friend qt::access<controller>;

  int seen = 0;

private:
  [[= qt::slot]] void onThing(int n)
  {
    seen += n;
  }

  [[= qt::prop{}]] int count = 0;
};
```

The line opens private slots, invocables, properties, accessors, signal members, timer
handlers and enums at once. A class whose annotated members are all public needs nothing.
A base carrying private members writes its own line, because a member is spliced through
the `access` of the class that declares it.

A signal is the one member whose access does not reach the metaobject: it is published
public wherever it is declared, because `Q_SIGNALS` is a public access specifier and moc has
no spelling for a private signal. That is not cosmetic. The QML engine's property cache
reads the access flag, and refuses to bind `on<Signal>` to anything else, with
`Cannot assign to non-existent property` and nothing on stderr.

Without it the class does not compile, and the diagnostic names the member and the line to
add:

```
error: ... 'what()': 'reflex.qt cannot reach controller::onThing:
                      add 'friend reflex::qt::access<controller>;' to controller'
```

---

## Class infos

`[[= qt::classinfo{key, value}]]` on the class becomes a `Q_CLASSINFO` entry, one
per annotation, in declaration order.

```cpp
struct [[= qt::classinfo{"author", "reflex"}]] described
    : qt::object<described>
{
  [[= qt::prop{}]] int value = 0;
};
```

---

## Metatypes and QML

`qmltyperegistrar` reads the JSON document moc writes with `--output-json`. reflex.qt emits
the same document from reflection, so a reflex class can be registered with the QML engine
without moc running on anything.

Declare the classes a document describes with a module body, in the shape of
`REFLEX_PY_MODULE`:

```cpp
#include <reflex/qt/moc/export.hpp>

REFLEX_QT_MODULE(app_types, m)
{
  m.expose<^^app::controllers>();   // every reflex.qt class declared in the namespace
  m.expose<app::settings>();        // one class
}

int main(int argc, char** argv)
{
  return reflex::qt::moc::export_main<app_types>(argc, argv);
}
```

`export_main` reads the output path and the `-I` roots off the command line the build
passes. `write_metatypes<app_types>(path, opts)` is the same thing without the argument
parsing.

The body is a `consteval` function, so the list is a compile-time one. Nothing runs before
`main`, there is no registry, and a class appears in the document because a body named it.
A namespace sweep takes the classes declared directly in the namespace, in declaration
order, and does not recurse: name a nested namespace to reach into it.

`expose` rejects, at the call, anything that is not a complete class deriving
`qt::gadget<T>` or `qt::object<T>`, and any class with a published member that
[`qt::access<T>`](#private-members) cannot splice. The second is deliberate: a document
describing fewer members than the `QMetaObject` in the same binary is worse than no
document.

### QML types

`[[= qt::qml{}]]` publishes a class to QML. One aggregate carries every option, the way
`prop{}` does, so the options compose instead of needing a name per combination:

```cpp
struct [[= qt::qml{}]] controller : qt::object<controller>
{
  [[= qt::prop{}]] int count = 0;
};

struct [[= qt::qml{.singleton = true}]] settings : qt::object<settings> { };

struct [[= qt::qml{.name = "Gauge", .uncreatable = "ask Factory"}]] meter
    : qt::object<meter> { };

struct [[= qt::qml{.name = "span"}]] span : qt::gadget<span> { };
```

Each field is the class info moc writes for the macro it stands for, read off a real moc
run rather than off the macro definitions:

| field | moc macro | class info |
|---|---|---|
| none | `QML_ELEMENT` | `QML.Element` = `auto` |
| `.name = "Gauge"` | `QML_NAMED_ELEMENT(Gauge)` | `QML.Element` = `Gauge` |
| `.name = "span"` on a gadget | `QML_VALUE_TYPE(span)` | `QML.Element` = `span` |
| `.singleton = true` | `QML_SINGLETON` | `QML.Singleton` = `true` |
| `.uncreatable = "why"` | `QML_UNCREATABLE("why")` | `QML.Creatable` = `false` and `QML.UncreatableReason` = `why` |
| `.added_in = {2, 3}` | `QML_ADDED_IN_VERSION(2, 3)` | `QML.AddedInVersion` = `515` |
| `.removed_in = {3, 0}` | `QML_REMOVED_IN_VERSION(3, 0)` | `QML.RemovedInVersion` = `768` |

A version is one integer, `major * 256 + minor`. Major version `0` reads as no version
given, which is the default.

`qt::classinfo` reaches the same table, so a `QML_*` macro Qt adds later is expressible the
day it appears and needs no new field here:

```cpp
struct [[= qt::classinfo{"QML.HasCustomParser", "true"}]] parsed
    : qt::object<parsed> { };
```

### `reflex/qt/qml.hpp`

A singleton and an uncreatable type need more than a class info.
`qmlRegisterTypesAndRevisions` decides both from `QQmlPrivate::QmlSingleton<T>` and
`QQmlPrivate::QmlUncreatable<T>`, which read a nested enumeration and a marker member
function that `QML_SINGLETON` and `QML_UNCREATABLE` declare in the class body. Qt compares
the marker's owning class against `T`, so a CRTP base cannot supply either.
`reflex/qt/qml.hpp` specializes the two traits from the same annotation instead.

Include it from the header the QML module registers. It stays out of `reflex/qt.hpp`,
which pulls in QtCore alone:

```cpp
#include <reflex/qt/qml.hpp>
```

A class whose header does not reach it registers as an ordinary creatable type, and its
`QML.Singleton` class info goes unread.

### Building a QML module

`reflex_build.qt` declares the whole module on top of the builders pcons already exposes:
`add_metatypes` builds the exporter and `qml_module` hands its document to
`qmltyperegistrar`.

```python
from reflex_build.qt import add_metatypes, qml_module, use_qt

qt_qml = use_qt(project, env, ["Qml"])

metatypes, exporter = add_metatypes(
    "app", ["module.cpp"], link=[qt_qml.Qml], include_roots=["include"]
)

types = qml_module(
    "app-types", env,
    uri="Com.Example.App",
    qml_files=["app/Main.qml"],
    metatypes=metatypes,
    link=[qt_qml.Qml, project.get_target("reflex.qt")],
)
types.depends(exporter)
types.private.include_dirs.append("include")
```

`module.cpp` is the module body plus the one-line `main` above. `include_roots` are the
exporter's own include directories and the roots it spells `inputFile` relative to, so pass
the ones the QML module compiles the generated registration with.

`qml_module` is `project.QtQmlModule` without moc: it runs `qmltyperegistrar` on the
exporter's document, synthesizes the `qmldir`, embeds the QML files, the `qmldir` and the
generated `.qmltypes` under `:/qt/qml/<uri as path>/`, and returns an object target that
`app.link(types)` pulls in whole. `qml_files` are project-root relative, the way pcons takes
them.

`qmltyperegistrar` generates the registration and the module compiles it, so nothing is
hand-written. The generated file includes the header `inputFile` names and instantiates
`qmlRegisterTypesAndRevisions<T>`, which reads `QQmlPrivate::QmlResolved<T>`,
`QmlExtended<T>`, `QmlSingleton<T>`, `QmlInterface<T>`, `QmlSequence<T>`,
`QmlUncreatable<T>` and `QmlAnonymous<T>` alongside the metaobject. The primary templates
answer for a class that declares none of the corresponding macros, which is why an ordinary
reflex.qt class compiles there untouched, and why a singleton or an uncreatable type needs
[`reflex/qt/qml.hpp`](#reflexqtqmlhpp) on that include line.

Nothing merges moc's collection with the exporter's document, so one module publishes either
reflex types or `Q_OBJECT` types, not both. A moc'ed class gets its own module through
`project.QtQmlModule`.

One failure mode is worth knowing, because it is invisible in the document.
`qmlRegisterTypesAndRevisions` reads `QML.Element` back off the **runtime metaobject**, not
off the JSON, and a class missing it fails at load with *Missing QML.Element class info*
while its `.qmltypes` looks perfect. reflex.qt feeds the blob and the document from the one
annotation, so the two cannot disagree here.

The exporter needs to be told where the compiler ran. ninja compiles from the build
directory with relative paths, so `source_location_of(...).file_name()` records one, and the
binary carries nothing that says what it was relative to. `add_metatypes` passes
`-C <build dir>` for that; a hand-run exporter must pass it too, from any working directory:

```console
$ ./build/qt/reflex-qt-clock-export - -I qt/examples
cannot resolve ../qt/examples/clock/types.hpp: the compiler recorded a relative path,
pass -C <the directory the compiler ran in>
$ ./build/qt/reflex-qt-clock-export - -C build -I qt/examples   # writes the document
```

It refuses to guess rather than write a spelling nothing can include, and a path that
resolves to no file is an error on the same grounds.

### `inputFile`

`qmltyperegistrar` writes `#if __has_include(<inputFile>)` into the registration it
generates, so the path has to resolve as an angled include on the line that compiles that
file. With no root matching, the spelling is the recorded path completed with `compile_dir`
and canonicalized, so an absolute path off the machine that built it. Name the include
directories the consumer will pass to get the short, relocatable spelling instead:

```cpp
reflex::qt::moc::write_metatypes<app_types>(
    argv[1], {.include_roots = {"include"}, .compile_dir = "/path/the/compiler/ran/in"});
// /src/app/include/app/thing.hpp  ->  app/thing.hpp
```

`compile_dir` is what `-C` fills in `export_main`. It can be left empty only where the
compiler recorded absolute paths.

### What the document says

The fields are moc's schema, not reflex's, and the values come from the same reflections the
`QMetaObject` is built from rather than from a readback of it. So a property carries the
real names of its `getter` and `setter` where moc carries `READ` and `WRITE`, a property
with neither carries `member` the way moc's `MEMBER` does, a slot and an invocable carry
their parameter names, and `constant`, `final` and `required` come from the `prop{}`
annotation. An absent field is left out rather than written as `null`, which is what moc
does and what `qmltyperegistrar` reads.

`qt/tests/moc-cross-check.py` is the proof: it runs real moc on `qt/tests/moc-mirror.hpp`
and the exporter on the equivalent reflex classes, normalizes both and diffs them, then
runs `qmltyperegistrar` on each document and diffs the two `.qmltypes`. The pair covers an
object, a QML singleton, a value-type gadget, a class deriving another reflex.qt class, one
declaring its own flags and a protected slot, and one under `naming::qt_style`, and both
diffs are empty once the signal parameter names below are dropped from each side.
`pcons test` runs it as `qt.moc-cross-check` and skips it where Qt's tools are not
installed.

---

## Differences from moc

Five, all understood. Two are observable through the `QMetaObject` API: the enumeration
descriptors and the signal parameter names. The other three show up in the metatypes
document or nowhere.

- **String table order.** moc orders the string table by first use, this orders it by kind.
  Both tables hold the same strings and every reference into them is an index, so nothing
  reads differently.
- **`EnumOrFlag` on a property of a custom class type.** moc sets it on every property
  whose type it cannot resolve, because it cannot tell an enumeration from a struct.
  Reflection can, so the flag is set only for an actual enumeration. `isEnumType()` reads
  `false` on both sides.
- **One descriptor per nested enumeration.** moc publishes what `Q_ENUM` and `Q_FLAG` mark.
  This publishes every nested enumeration and every `QFlags` alias over one, so a class with
  a flag alias gets one more descriptor than moc emits for the same shape, and
  `QMetaObject::enumeratorCount()` reads the difference. It is a superset of what the macros
  mark, not of what moc can emit: `Q_DECLARE_FLAGS(Options, Option)` plus `Q_ENUM(Option)`
  plus `Q_FLAG(Options)` gets moc to the same pair, measured. Only the alias spelling loses
  it - `Q_ENUM(E)` and `Q_FLAG(QFlags<E>)` over a `using` alias yields a single descriptor
  and drops the flag entry.
- **Signal parameter names.** Empty, where moc has them. See
  [Not supported yet](#not-supported-yet).
- **The spelling of a type in the metatypes document.** moc echoes a property's or a
  parameter's type as the author wrote it, so `Q_PROPERTY(Mode mode ...)` reads
  `"type": "Mode"` and `Q_PROPERTY(mirror::Mode mode ...)` reads `"type": "mirror::Mode"`.
  Reflection cannot see the spelling, so the document always carries the qualified name.
  `qmltyperegistrar` resolves either, and the blob is unaffected: it spells a nested
  enumeration unqualified on both sides. `qt/tests/moc-cross-check.py` drops the
  qualification explicitly rather than letting the mirror hide the difference.

---

## Utilities

### `connection_guard`

Disconnects what it holds when it goes out of scope. Move-only, since a copy would
disconnect the same connection twice.

```cpp
emitter sender;
{
  qt::connection_guard guard = QObject::connect(&sender, &emitter::ping, [] { });
  sender.ping();
}
sender.ping();   // nothing runs, the guard disconnected on scope exit
```

`release()` hands the connection out and leaves the guard empty. `reset()` drops what it
holds and optionally takes another connection.

### `QString` formatting

`reflex/qt/format.hpp`, pulled in by the umbrella header, formats a `QString` as UTF-8
through the full string format spec.

```cpp
std::format("{}", QString{"hello"});     // "hello"
std::format("{:>7}", QString{"hi"});     // "     hi"
std::format("{:.2}", QString{"hello"});  // "he"
```

Narrow only. A `wchar_t` specialization would transcode UTF-16 into a unit that is 4 bytes
on Linux and 2 on Windows, so `std::format(L"{}", s)` is an error where it is written
rather than platform-dependent output.

### `describe` and `dump`

`reflex/qt/debug.hpp` renders a metaobject's class infos, methods, properties and enums.
The walk is a runtime one over `QMetaObject`, so it reads a reflex class and a real moc'ed
one the same way. It is deliberately not in `reflex/qt.hpp`, so including the module does
not drag `<print>` into every translation unit.

```cpp
#include <reflex/qt/debug.hpp>

const std::string text = qt::describe<counter>();
// class counter
//   signal      changed(int)
//   signal      valueChanged()
//   slot        bump()
//   property    value : int

qt::dump(some_qobject);   // the same text, to stdout
```

---

## Building

`reflex.qt` is a header-only pcons target that carries its Qt dependency, so a consumer
links it and nothing else.

```python
from reflex_build.qt import use_qt

qt  = use_qt(project, env, ["Core", "Widgets"])
app = project.QtProgram("app", env, sources=["main.cpp"], link=[qt.Widgets])
app.link(project.get_target("reflex.qt"))
```

`use_qt` returns what `find_qt` returns, `None` included, and it defaults to
`required=False`. When Qt is absent `qt/pcons-build.py` gets that `None` and skips the
module, leaving the rest of the build green.

pcons scans for `Q_OBJECT`, `Q_GADGET` and `Q_NAMESPACE` to decide what needs moc. A
reflex.qt class carries none of them, so the scan finds nothing and no moc edge is emitted.
`automoc=False` is not needed.

Call `use_qt` rather than `find_qt`: two flags are load-bearing on this toolchain and
`reflex_build/qt.py` applies them there, so a build that reaches `find_qt` directly gets
neither. Qt's include directories move to `system_include_dirs`, because Qt 6.11 headers
trip `-Wsfinae-incomplete` on GCC 16 and `-Werror` makes that fatal. The Qt modules get
`-fPIC`, because this Qt is built `-reduce-relocations` and linking without it fails with a
copy relocation against `QByteArray::_empty`.

The examples under `qt/examples/` build with `pcons REFLEX_BUILD_PROGRAMS=true` and run
with

```console
$ pcons run qt-example widgets
$ pcons run qt-example clock
$ pcons run qt-example sandbox
```

`clock` and `sandbox` are QML modules. `qt/examples/qml-check.py` runs both offscreen,
checks that each exits 0, prints its summary line and writes nothing to stderr, then lints
their QML against the `.qmltypes` the build generated:

```console
$ python3 qt/examples/qml-check.py build
```

---

## Qt version

Qt **6.10.x** and **6.11.x**. `detail/version.hpp` asserts the window:

```
static assertion failed: reflex.qt reproduces moc's private metaobject layout and is
only tested against Qt 6.10.x and 6.11.x; configure with
REFLEX_QT_ALLOW_UNTESTED_QT=true to try anyway
```

Both are measured, not assumed: the suite and `qt.moc-cross-check` run green on 6.11.1
against the host's moc and on 6.10.2 against the CI image's. The two versions differ in one
place, and only in the metatypes document: moc gained a property's `override` and `virtual`
keys in 6.11, so `reflex.qt` writes them there and leaves them out on 6.10.
`qmltyperegistrar` reads neither, and the `.qmltypes` generated from the two documents are
identical.

The pin is not caution. `reflex.qt` fills `QtMocHelpers::UintData`, `FunctionData`,
`PropertyData`, `EnumData` and `StringRefStorage` and specializes
`QtPrivate::HasQ_OBJECT_Macro`, `QtPrivate::FunctionPointer`, `QtPrivate::IsGadgetHelper`
and `QMetaTypeId`. Those are Qt's own internals with no compatibility promise, and the
metaobject data layout has changed between Qt minor releases before. `qtmochelpers.h` and
`qtmocconstants.h` are public headers in 6.11.1, so no `private_headers=` is needed, but
the layout they describe is still unstable.

`pcons REFLEX_QT_ALLOW_UNTESTED_QT=true` turns the assertion off, through `reflex_build`'s
`get_var` like the other build options. Widen the window in the header only after actually
building and running the suite against another Qt, as 6.10 was.

---

## Not supported yet

**The QML macros that declare a type rather than a class info.** `QML_ATTACHED`,
`QML_EXTENDED`, `QML_FOREIGN` and `QML_SEQUENTIAL_CONTAINER` each declare a nested typedef
and a marker member function that Qt reads back off the class; `QML_INTERFACE` declares a
nested enumeration and a marker instead, the same shape `QML_SINGLETON` has. `qt::qml`
covers the singleton and uncreatable pair; the rest would each need another `QQmlPrivate`
specialization in `reflex/qt/qml.hpp`. The class infos are reachable through
`qt::classinfo` meanwhile, which is enough for the `.qmltypes` and not enough for the
engine.

**A QML import path on disk.** A pcons QML module lives in the binary's resources, so
`qmllint` and `qmlls` need a directory tree with a `qmldir` in it.
`qt/examples/qml-check.py` stages one in a temporary directory for the lint run, and
nothing writes a `.qmlls.ini`.

**C++ modules.** Qt and C++ named modules do not work together on this toolchain, so
`reflex.qt` ships as headers rather than as a `.cppm`, alone among the reflex modules. Its
headers include `reflex/meta.hpp`, `reflex/constant.hpp` and friends rather than importing
them. `reflex.core` and `serde` expose usable headers alongside their module interfaces, so
nothing is missing.

**Constructors.** `qt_constructors` is empty, so `QMetaObject::newInstance` finds nothing
and no constructor is published. QML instantiates a registered type through C++ rather than
through the metaobject, so this costs nothing there, but `QML_CONSTRUCTIBLE_VALUE` and
`QML_STRUCTURED_VALUE` build a value type from its published constructors and cannot work
without them.

**Computed properties.** A property is a data member. A property backed only by accessors,
with no storage, is not expressible: a getter and a setter are optional decorations on a
member, not a substitute for one.

**Signal parameter names.** `QMetaMethod::parameterNames()` returns empty strings for a
signal. A `signal<int, qt::defaulted<int>>` carries types and not names, which is a
permanent consequence of declaring a signal as a data member. Slot and invocable parameter
names are emitted and match moc. This is cosmetic for `connect` and `invokeMethod` and
shows only in tooling.

**Disconnecting a signal-to-signal chain by naming the target signal.** Chaining works,
`connect(&a, &A::sig, &b, &B::sig)` delivers, but the matching `disconnect` overload
returns `false` and disconnects nothing. Qt compares the stored slot object against a
member function pointer, and a reflex signal is a data member. Keep the
`QMetaObject::Connection` and disconnect that instead.

```cpp
const auto chain = QObject::connect(&sender, &emitter::pair, &receiver, &emitter::pair);
QObject::disconnect(&sender, &emitter::pair, &receiver, &emitter::pair);  // false
QObject::disconnect(chain);                                              // true
```

**A gadget inheriting a gadget.** `struct derived : qt::gadget<derived>, base_g` makes
`staticMetaObject` ambiguous between the two bases and does not compile, and
`struct derived : base_g`, the only other spelling, never instantiates `gadget<derived>`: it
inherits `base_g`'s metaobject whole, so `derived::staticMetaObject` is `base_g`'s, down to
its `className()`, and the properties `derived` declares are invisible to Qt. Nothing
diagnoses it, because no reflex code runs for a class that does not name the CRTP base. Its
metatype is still its own, so `QVariant` does not confuse the two. An object inherits
properly through `qt::object<derived, base>`; a gadget has no equivalent.

**An opt-out for a published enumeration.** Every nested enumeration is published. There is
no `qt::skip`, so a private implementation enum reaches the metaobject too.

**`QProperty` bindings.** `BindableProperty` is answered the way moc answers it for a
non-bindable property. Nothing is `QBindable`.

**Anything but Linux and GCC 16.2.1.** The module is written against a compiler with
working C++26 reflection and has been built and run nowhere else.

---

> See [tests](tests) for more examples, [examples/widgets](examples/widgets) for a working
> application, and [examples/clock](examples/clock) and
> [examples/sandbox](examples/sandbox) for QML modules.
