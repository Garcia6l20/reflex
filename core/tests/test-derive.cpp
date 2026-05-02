#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;

enum class[[= derive{Parse, Format}]] ParsableEnum
{
  A = 1,
  B = 2,
  C = 3
};

static_assert(reflex::derives(^^ParsableEnum, Parse));
static_assert(reflex::derives_c<ParsableEnum, decltype(Parse)>);
static_assert(reflex::derives(^^ParsableEnum, Format));
static_assert(reflex::derives_c<ParsableEnum, decltype(Format)>);


enum class ExtParsableEnum
{
  A = 1,
  B = 2,
  C = 3
};

template <>
constexpr bool reflex::derives_v<ExtParsableEnum, Parse> = true;

template <>
constexpr bool reflex::derives_v<ExtParsableEnum, Format> = true;

static_assert(reflex::derives(^^ExtParsableEnum, Parse));
static_assert(reflex::derives(^^ExtParsableEnum, Format));
