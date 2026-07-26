module;

// serde.hpp is included in the module purview below, so mmap_input_stream's
// POSIX headers cannot go with it: a textual include of a C header inside a
// module purview attaches its declarations to the module. They belong here, in
// the global module fragment, where they stay attached to the global module.
// `import std;` exports no macros, so errno and the E* constants have to come
// from a real include even though every other name here does not. That one is
// needed on both branches, mapping or fallback.
#include <cerrno>

#if defined(__unix__) or defined(__APPLE__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

export module reflex.serde;

export import reflex.poly;

import std;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/serde.hpp>
