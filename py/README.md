# reflex.py

> **Python bindings for C++26**, powered by static reflection.

Write the class. Say which type to publish. `reflex.py` reads the constructors,
methods, overloads, data members, operators, bases and nested types off the
declaration and hands them to [nanobind](https://github.com/wjakob/nanobind).

No per-member `.def` calls, no registration list to keep in step with the header.

---

## Quick start

```cpp
#include <reflex/py.hpp>

namespace py = reflex::py;

struct some_type;                       // declared elsewhere, never defined here

class my_class
{
public:
  my_class(int a, int b);
  my_class(some_type const&);           // dropped, some_type is incomplete
  int method();
  int method(int n);
  int method(some_type const&);         // dropped, same reason
  [[= py::skip]] int non_python_method();

private:
  int state_;                           // never published
};

REFLEX_PY_MODULE(my_ext, m)
{
  m.bind<my_class>();
}
```

```python
import my_ext

o = my_ext.my_class(2, 3)
o.method()
o.method(n=7)
```

## What gets published

| C++ | Python |
|---|---|
| every constructor, one per reachable arity | `__init__` overloads |
| member function, and each overload of it | method, one `def` per candidate |
| static member function | `staticmethod` |
| public data member | read-write property |
| `const` data member | read-only property |
| static data member | class-level attribute |
| public non-virtual base | Python base class, bound first |
| nested class or enumeration | attribute of the enclosing Python type |
| operator | the dunder the table gives it, see below |
| `explicit operator bool` / `std::string` | `__bool__` / `__str__` |

A private member, an inaccessible base and anything mentioning an incomplete
type are absent. Access is read from outside the class, so what Python sees is
what a caller outside the class would see.

## Annotations

```cpp
struct [[= py::doc{"what it is"}]] [[= py::naming::snake_case]] widget
{
  [[= py::readonly]] int serial;
  [[= py::rename{"count"}]] int howMany;
  [[= py::skip]] int internal;

  [[= py::doc{"what it does"}]] int spin() const;
  [[= py::returns{py::nb::rv_policy::reference}]] widget* find(int id);
};
```

| Annotation | Applies to | Effect |
|---|---|---|
| `py::skip` | anything | keep it out of the Python surface |
| `py::rename{"name"}` | member, operator | the Python name, a dunder included |
| `py::doc{"text"}` | class, enum, member | the docstring |
| `py::readonly` | data member | expose without a setter |
| `py::naming::<case>` | class, enum, namespace | how its members are spelled |
| `py::returns{policy}` | member function | what nanobind does with the result |
| `py::submodule` | nested namespace | follow it when binding the parent |

A `py::naming` on a scope governs what is inside it, never what the scope itself
is called. A `py::rename` on one overload renames the whole set.

## Binding a namespace

```cpp
REFLEX_PY_MODULE(my_ext, m)
{
  m.bind<^^mylib>();
}
```

Publishes every public class, enumeration, free function and `const` value in
`mylib`. A nested namespace is followed only when it carries `[[= py::submodule]]`.
Types are bound before functions, because a class names its base at registration.

`m.bind_enum<E>()` publishes an enumeration that is not nested in a bound class.

## Operators

The Python name is a table, not a rule. `operator+` is `__add__` written binary
and `__pos__` written unary. `operator<=>` produces all six comparisons at once,
unless the class declares its own `operator==`, which is more specific and wins
the equality pair. A non-const `operator[]` returning a reference also produces
`__setitem__`.

Left unbound: `operator=`, `operator->`, unary `operator*`, `operator!`,
`operator++`, `operator--`, `operator&&`, `operator||`, `operator,`.
`py::rename{"__dunder__"}` reaches any of them.

## Return values

A member returning an lvalue reference to a class hands back a view of a
subobject, kept alive by the object it came from. Mutating the result mutates
the original. Everything else is left to nanobind: a returned pointer is taken
over, a returned value is moved.

A pointer to something borrowed and a pointer to a fresh allocation are the same
type, so that case needs `py::returns`.

## Limits

- **Function templates are not bound.** A template has no parameter types until
  it is substituted. It is dropped without a diagnostic, because a class not
  written for Python commonly has one.
- **A default argument becomes an extra overload, not a default.** Reflection
  exposes that a parameter has a default, not what the default is, so
  `scaled(int n, int k = 2)` binds as `scaled(n)` plus `scaled(n, k)`. Both
  calls work; `help()` shows two signatures.
- **Constructor arguments are positional only.** `nb::init` carries types, not
  names.
- **Docstrings come from `py::doc` alone.** A `///` comment is not reachable
  through reflection.
- **STL types need their caster included by you.** `#include <nanobind/stl/string.h>`
  and friends. Reflection cannot add an include.
- **A C array data member is not published.** There is no caster for one and it
  cannot be assigned through.
- **One base only.** nanobind's `class_` carries a single base; a second bindable
  one is a compile-time error. Use `py::skip` on the others.
- **A free operator is not reached.** Only operators declared in the class are
  seen. This is the same boundary `reflex::overloads_of` documents.
- **An enumerator cannot be annotated on GCC 16.** The compiler rejects it. A
  `py::naming` on the enumeration is the only way to respell enumerators.
- **A parameter named `self` on an instance method is a compile-time error.**
  nanobind names the object `self`, so the argument would be unreachable by
  keyword. A deducing-this object parameter named `self` is fine, and so is one
  on a free or static function.

## Building an extension

An extension includes the headers; it does not `import reflex.py`. The binder is
entirely `consteval` and `inline`, so there is nothing to link.

```
python3 -m pcons.packages.fetch.cli fetch deps.toml \
    --deps-dir build/deps --output-dir build/pkg
REFLEX_BUILD_TESTS=1 pcons
pcons build test-py
```

`reflex_build/python.py` exposes `nanobind_library`, `add_python_extension` and
`add_stub`. `pcons build py-stubs` writes a `.pyi` next to each extension with
nanobind's own `stubgen`.

### CMake

`reflex::py` is exposed but off by default, and nothing is fetched. Point CMake
at an existing nanobind and turn it on:

```
cmake -S . -B build -DREFLEX_PY_ENABLED=ON \
      -Dnanobind_ROOT=$(python3 -m nanobind --cmake_dir)
```

Building an extension is nanobind's job. `reflex::py` only adds the binder:

```cmake
find_package(reflex REQUIRED)
find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(my_ext my_ext.cpp)
target_link_libraries(my_ext PRIVATE reflex::py)
```

Find Python before nanobind: `nanobind_add_module` compiles nanobind's own
runtime and needs `Python.h`.
