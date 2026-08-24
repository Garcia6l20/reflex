---
name: Bug report
about: reflex does something wrong, or does not compile
title: ''
labels: bug
assignees: ''
---

## What happens

What you observe, and what you expected instead.

## Where

- Module: core, cli, poly, serde, py, jinja or qt
- reflex version or commit:

## Toolchain

- `g++ --version`:
- Build: the pcons command line, or the compile command if you build by hand
- Variant: release or debug
- Operating system:

GCC 16.0.1, which Ubuntu 26.04 ships, miscompiles the reflection code and is
not supported. Please check the version before reporting.

## Reproducer

The smallest program that shows it, complete enough to compile as it stands.

```cpp
```

## Output

The error or the wrong behaviour, pasted whole. A reflection diagnostic is
long, keep it that way, and keep the `In substitution of` lines.

```
```

## What you already ruled out

Workarounds you tried, and what changed. Say so if you suspect a compiler bug
rather than a reflex one.
