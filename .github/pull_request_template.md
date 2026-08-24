## Summary

What this changes, and why.

### Details

The changes, one per line.

1. (...)
2. (...)

## Related issues

Closes #

## How to test

The commands a reviewer runs to see it, on top of the suite below. Name the
test binaries this adds or changes.

```bash
pcons REFLEX_BUILD_TESTS=true
pcons test -L <group>
```

## Checklist

- [ ] Builds with GCC >= 16.1.0
- [ ] `pcons test` passes
- [ ] New behaviour comes with a test
- [ ] New public entities carry doxygen documentation
- [ ] README/CONTRIBUTING.md or module documentation updated if the public API changed

## Breaking changes

What stops compiling, and what it becomes. Leave empty if nothing does.

## Notes for the reviewer

Anything worth knowing before reading the diff: a compiler bug worked around,
an alternative that was tried and dropped, a part that is deliberately left
for later.
