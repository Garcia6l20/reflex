#include <doctest/doctest.h>

#include <reflex/const_check.hpp>
#include <reflex/qt.hpp>

/** @file
 *
 * The shapes reflex.qt refuses to publish, and the sentence it refuses them
 * with. Each case calls the consteval check the way the class body calls it,
 * since a class carrying the rejected shape would end the translation unit.
 */

namespace qtd = reflex::qt::detail;
namespace qt  = reflex::qt;

TEST_CASE("a defaulted<> signal argument that is not trailing is rejected")
{
  consteval
  {
    REFLEX_CONSTEVAL_NOTHROW(qtd::check_trailing_defaults({^^int, ^^qt::defaulted<int>}));
    REFLEX_CONSTEVAL_NOTHROW(
        qtd::check_trailing_defaults({^^qt::defaulted<int>, ^^qt::defaulted<int>}));
    REFLEX_CONSTEVAL_NOTHROW(qtd::check_trailing_defaults({}));

    REFLEX_CONSTEVAL_THROWS_WITH("has to be trailing",
                                 qtd::check_trailing_defaults({^^qt::defaulted<int>, ^^int}));
    REFLEX_CONSTEVAL_THROWS_WITH(
        "the signal argument int follows a defaulted<> one",
        qtd::check_trailing_defaults({^^qt::defaulted<int>, ^^int, ^^qt::defaulted<int>}));
  }
}
