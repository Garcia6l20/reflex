// Hand-written nanobind, deliberately. This proves the build: the include
// paths, the extension suffix, the undefined CPython symbols and the import.
// Mixing reflection in would make a link failure and a reflection failure look
// the same.
#include <nanobind/nanobind.h>

namespace nb = nanobind;

NB_MODULE(hello, m)
{
  m.def("add", [](int a, int b) { return a + b; });
}
