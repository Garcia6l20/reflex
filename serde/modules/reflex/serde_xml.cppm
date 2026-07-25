module;

#include <cstring>

// load_file maps the file where the platform can; these have to be in the
// global module fragment, not the module purview.
#if defined(__unix__) or defined(__APPLE__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

export module reflex.serde.xml;

import std;

export import reflex.core;
export import reflex.serde;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/serde/xml.hpp>
