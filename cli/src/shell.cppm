module;

#if __has_include(<termios.h>) && __has_include(<unistd.h>)
#include <termios.h>
#include <unistd.h>
#else
#error "Terminal handling requires <termios.h> and <unistd.h>"
#endif

export module reflex.shell;

export import reflex.cli;

import std;

#define REFLEX_MODULE

#define REFLEX_EXPORT       export
#define REFLEX_BEGIN_EXPORT export {
#define REFLEX_END_EXPORT   }

#include <reflex/shell.hpp>
