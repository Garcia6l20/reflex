#include <doctest/doctest.h>

#include <reflex/const_check.hpp>
#include <reflex/qt.hpp>

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>

/** @file
 *
 * The shapes reflex.qt refuses to publish, and the sentence it refuses them
 * with. Each case calls the consteval check the way the class body calls it,
 * since a class carrying the rejected shape would end the translation unit.
 */

namespace qtd = reflex::qt::detail;
namespace qt  = reflex::qt;

struct slotted_gadget : qt::gadget<slotted_gadget>
{
  [[= qt::prop{}]] int x = 1;

  [[= qt::invocable]] int twice() const
  {
    return 2 * x;
  }

  [[= qt::slot]] void bump()
  {
    ++x;
  }
};

struct signalling_gadget : qt::gadget<signalling_gadget>
{
  qtd::signal_decl<signalling_gadget, int> changed{this};
};

struct timed_gadget : qt::gadget<timed_gadget>
{
  void tick()
  {
  }

  qt::timer<^^tick> ticker;
};

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

TEST_CASE("a gadget publishes a slot the way moc does")
{
  const QMetaObject& mo = slotted_gadget::staticMetaObject;

  REQUIRE(mo.methodCount() == 2);
  CHECK(mo.method(0).methodSignature() == QByteArray{"bump()"});
  CHECK(mo.method(0).methodType() == QMetaMethod::Slot);
  CHECK(mo.method(1).methodSignature() == QByteArray{"twice()"});
  CHECK(mo.method(1).methodType() == QMetaMethod::Method);

  slotted_gadget g;
  REQUIRE(mo.method(0).invokeOnGadget(&g));
  CHECK(g.x == 2);
}

TEST_CASE("a signal or a timer on a gadget is rejected")
{
  consteval
  {
    REFLEX_CONSTEVAL_NOTHROW(qtd::validate_gadget_members(^^slotted_gadget));

    REFLEX_CONSTEVAL_THROWS_WITH("has no QMetaObject::activate to emit it",
                                 qtd::validate_gadget_members(^^signalling_gadget));
    REFLEX_CONSTEVAL_THROWS_WITH("has no timerEvent to dispatch it",
                                 qtd::validate_gadget_members(^^timed_gadget));
  }
}
