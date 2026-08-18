#include <doctest/doctest.h>

#include <reflex/const_check.hpp>
#include <reflex/qt.hpp>

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <string_view>

struct accessed : reflex::qt::object<accessed>
{
  [[= prop{}]] int p1    = 0;
  [[= prop{}]] int plain = 0;

  [[= getter{"p1"}]] int getP1() const
  {
    return p1 * 2;
  }

  [[= setter{"p1"}]] void setP1(int value)
  {
    p1 = value / 2;
  }

  [[= listener{"p1"}]] void onP1Changed()
  {
    ++listener_calls;
    listener_saw_notification = notifications;
  }

  int listener_calls            = 0;
  int listener_saw_notification = -1;
  int notifications             = 0;
};

struct[[= reflex::qt::naming::qt_style]] conventional : reflex::qt::object<conventional>
{
  [[= prop{}]] int p1 = 0;

  int getP1() const
  {
    return p1 * 3;
  }

  void setP1(int value)
  {
    p1 = value / 3;
  }

  void onP1Changed()
  {
    ++listener_calls;
  }

  int listener_calls = 0;
};

struct[[= reflex::qt::naming::qt_style]] annotation_wins : reflex::qt::object<annotation_wins>
{
  [[= prop{}]] int p1 = 0;

  int getP1() const
  {
    return -1;
  }

  [[= getter{"p1"}]] int readP1() const
  {
    return p1 * 10;
  }
};

struct locked : reflex::qt::object<locked>
{
  [[= prop{.write = false}]] int frozen = 7;
  [[= prop{}]] int               open   = 0;
};

struct flagged : reflex::qt::gadget<flagged>
{
  [[= prop{.write = false, .constant = true}]] int fixed = 3;
  [[= prop{.final = true, .required = true}]] int  both  = 0;
};

namespace bad
{
struct unknown_property : reflex::qt::object<unknown_property>
{
  [[= prop{}]] int p1 = 0;

  [[= getter{"typo"}]] int getP1() const
  {
    return p1;
  }
};

struct duplicate_getter : reflex::qt::object<duplicate_getter>
{
  [[= prop{}]] int p1 = 0;

  [[= getter{"p1"}]] int getP1() const
  {
    return p1;
  }

  [[= getter{"p1"}]] int readP1() const
  {
    return p1;
  }
};

struct mismatched_setter : reflex::qt::object<mismatched_setter>
{
  [[= prop{}]] int p1 = 0;

  [[= setter{"p1"}]] void setP1(QString const&)
  {}
};

struct mismatched_getter : reflex::qt::object<mismatched_getter>
{
  [[= prop{}]] int p1 = 0;

  [[= getter{"p1"}]] QString getP1() const
  {
    return {};
  }
};

struct listening_to_nothing : reflex::qt::object<listening_to_nothing>
{
  [[= prop{.notify = false}]] int p1 = 0;

  [[= listener{"p1"}]] void onP1Changed()
  {}
};

struct neither_readable_nor_writable : reflex::qt::object<neither_readable_nor_writable>
{
  [[= prop{.read = false, .write = false}]] int p1 = 0;
};
} // namespace bad

namespace qtd = reflex::qt::detail;

TEST_CASE("a property or accessor that cannot be honoured is rejected at compile time")
{
  consteval
  {
    REFLEX_CONSTEVAL_NOTHROW(qtd::validate_properties(^^accessed));
    REFLEX_CONSTEVAL_NOTHROW(qtd::validate_properties(^^conventional));

    REFLEX_CONSTEVAL_THROWS(qtd::validate_properties(^^bad::unknown_property));
    REFLEX_CONSTEVAL_THROWS(qtd::validate_properties(^^bad::duplicate_getter));
    REFLEX_CONSTEVAL_THROWS(qtd::validate_properties(^^bad::mismatched_setter));
    REFLEX_CONSTEVAL_THROWS(qtd::validate_properties(^^bad::mismatched_getter));
    REFLEX_CONSTEVAL_THROWS(qtd::validate_properties(^^bad::listening_to_nothing));
    REFLEX_CONSTEVAL_THROWS(qtd::validate_properties(^^bad::neither_readable_nor_writable));

    REFLEX_CONSTEVAL_THROWS(
        qtd::accessor_for<qtd::getter>(
            ^^bad::duplicate_getter, qtd::property_named(^^bad::duplicate_getter, "p1")));
  }
}

TEST_CASE("accessor resolution picks the annotation, then the convention, then nothing")
{
  static_assert(
      qtd::accessor_for<qtd::getter>(^^conventional, qtd::property_named(^^conventional, "p1"))
      != reflex::meta::null);
  static_assert(
      qtd::accessor_for<qtd::getter>(^^accessed, qtd::property_named(^^accessed, "plain"))
      == reflex::meta::null);
  static_assert(
      identifier_of(
          qtd::accessor_for<qtd::getter>(
              ^^annotation_wins, qtd::property_named(^^annotation_wins, "p1")))
      == "readP1");
}

TEST_CASE("a custom getter answers every read path")
{
  accessed a;
  a.p1 = 5;

  CHECK(a.property<"p1">() == 10);
  CHECK(a.property<^^accessed::p1>() == 10);
  CHECK(a.property("p1").toInt() == 10);
}

TEST_CASE("a property without a getter reads its member")
{
  accessed a;
  a.plain = 4;

  CHECK(a.property<"plain">() == 4);
  CHECK(a.property<^^accessed::plain>() == 4);
  CHECK(a.property("plain").toInt() == 4);
}

TEST_CASE("a custom setter answers every write path")
{
  accessed a;

  a.setProperty<"p1">(10);
  CHECK(a.p1 == 5);

  a.setProperty<^^accessed::p1>(20);
  CHECK(a.p1 == 10);

  REQUIRE(a.setProperty("p1", 30));
  CHECK(a.p1 == 15);
}

TEST_CASE("a write that changes nothing notifies nothing")
{
  accessed a;
  int      notified = 0;

  QObject::connect(&a, &accessed::propertyChanged<"plain">, [&notified] { ++notified; });

  a.setProperty<"plain">(3);
  CHECK(notified == 1);

  a.setProperty<"plain">(3);
  CHECK(notified == 1);

  a.setProperty<"plain">(4);
  CHECK(notified == 2);
}

TEST_CASE("the listener runs after the write and before the notify signal")
{
  accessed a;

  QObject::connect(&a, &accessed::propertyChanged<"p1">, [&a] { ++a.notifications; });

  a.setProperty<"p1">(10);

  CHECK(a.p1 == 5);
  CHECK(a.listener_calls == 1);
  CHECK(a.listener_saw_notification == 0);
  CHECK(a.notifications == 1);
}

TEST_CASE("a read-only property is described and enforced as read-only")
{
  const QMetaObject& mo     = locked::staticMetaObject;
  const int          offset = mo.propertyOffset();

  const auto frozen = mo.property(offset + 0);
  CHECK(std::string_view{frozen.name()} == "frozen");
  CHECK(frozen.isReadable());
  CHECK(not frozen.isWritable());

  CHECK(mo.property(offset + 1).isWritable());

  locked l;
  CHECK(not l.setProperty("frozen", 9));
  CHECK(l.frozen == 7);
  CHECK(l.property("frozen").toInt() == 7);
  CHECK(l.property<"frozen">() == 7);
}

TEST_CASE("the Qt property flags follow the annotation")
{
  const QMetaObject& mo = flagged::staticMetaObject;

  const auto fixed = mo.property(0);
  CHECK(fixed.isConstant());
  CHECK(not fixed.isWritable());
  CHECK(not fixed.isFinal());

  const auto both = mo.property(1);
  CHECK(both.isFinal());
  CHECK(both.isRequired());
  CHECK(not both.isConstant());
}

TEST_CASE("convention mode finds the accessors with no annotation at all")
{
  conventional c;

  c.setProperty<"p1">(9);
  CHECK(c.p1 == 3);
  CHECK(c.property<"p1">() == 9);
  CHECK(c.property("p1").toInt() == 9);
  CHECK(c.listener_calls == 1);
}

TEST_CASE("an annotated accessor wins over a conventionally named one")
{
  annotation_wins a;
  a.p1 = 2;

  CHECK(a.property<"p1">() == 20);
  CHECK(a.property("p1").toInt() == 20);
}
