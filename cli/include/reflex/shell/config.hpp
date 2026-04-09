#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <reflex/heapless/string.hpp>
#endif

REFLEX_EXPORT namespace reflex::shell
{
    template <std::size_t N>
    using string = heapless::string<N>;

    using line_type = string<256>;
}