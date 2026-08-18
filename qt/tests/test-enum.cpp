#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QFlags>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaProperty>
#include <QtCore/QVariant>

#include <optional>
#include <string_view>

struct palette : reflex::qt::gadget<palette>
{
  enum Color
  {
    Red,
    Green = 5,
    Blue
  };

  enum class Mode
  {
    Fast,
    Slow = 9
  };

  enum Option
  {
    NoOption = 0x0,
    First    = 0x1,
    Second   = 0x2
  };

  using Options = QFlags<Option>;

  enum class Wide : long long
  {
    Small = 1,
    Huge  = 1LL << 40
  };

  [[= prop{}]] Color   color = Red;
  [[= prop{}]] Mode    mode  = Mode::Fast;
  [[= prop{}]] Options opts;
};

struct signal_free : reflex::qt::gadget<signal_free>
{
  struct nested
  {
    int v = 0;
  };

  using alias   = int;
  using shorter = nested;

  [[= prop{}]] int x = 0;
};

namespace
{
namespace qtd = reflex::qt::detail;

static_assert(qtd::static_meta_type_id_of(^^palette::Color) == qtd::custom_type);
static_assert(qtd::static_meta_type_id_of(^^palette::Mode) == qtd::custom_type);
static_assert(qtd::static_meta_type_id_of(^^palette::Options) == qtd::custom_type);
static_assert(qtd::static_meta_type_id_of(^^palette::Wide) == qtd::custom_type);

int index_of_enum(const QMetaObject& mo, const char* name)
{
  return mo.indexOfEnumerator(name);
}
}

TEST_CASE("every nested enumeration reaches the meta object")
{
  const QMetaObject& mo = palette::staticMetaObject;

  REQUIRE(mo.enumeratorCount() == 5);
  CHECK(std::string_view{mo.enumerator(0).name()} == "Color");
  CHECK(std::string_view{mo.enumerator(1).name()} == "Mode");
  CHECK(std::string_view{mo.enumerator(2).name()} == "Option");
  CHECK(std::string_view{mo.enumerator(3).name()} == "Options");
  CHECK(std::string_view{mo.enumerator(4).name()} == "Wide");
}

TEST_CASE("an unscoped enumeration is described as moc would describe it")
{
  const QMetaObject& mo = palette::staticMetaObject;
  const QMetaEnum    e  = mo.enumerator(index_of_enum(mo, "Color"));

  REQUIRE(e.isValid());
  CHECK(std::string_view{e.name()} == "Color");
  CHECK(std::string_view{e.scope()} == "palette");
  CHECK(not e.isScoped());
  CHECK(not e.isFlag());
  CHECK(e.metaType() == QMetaType::fromType<palette::Color>());

  REQUIRE(e.keyCount() == 3);
  CHECK(std::string_view{e.key(0)} == "Red");
  CHECK(e.value(0) == 0);
  CHECK(std::string_view{e.key(1)} == "Green");
  CHECK(e.value(1) == 5);
  CHECK(std::string_view{e.key(2)} == "Blue");
  CHECK(e.value(2) == 6);
}

TEST_CASE("a scoped enumeration keeps its scoped flag")
{
  const QMetaObject& mo = palette::staticMetaObject;
  const QMetaEnum    e  = mo.enumerator(index_of_enum(mo, "Mode"));

  REQUIRE(e.isValid());
  CHECK(e.isScoped());
  CHECK(not e.isFlag());
  CHECK(e.metaType() == QMetaType::fromType<palette::Mode>());

  REQUIRE(e.keyCount() == 2);
  CHECK(std::string_view{e.key(0)} == "Fast");
  CHECK(e.value(0) == 0);
  CHECK(std::string_view{e.key(1)} == "Slow");
  CHECK(e.value(1) == 9);
}

TEST_CASE("a QFlags alias is published as a flag enumeration over its argument")
{
  const QMetaObject& mo = palette::staticMetaObject;
  const QMetaEnum    e  = mo.enumerator(index_of_enum(mo, "Options"));

  REQUIRE(e.isValid());
  CHECK(std::string_view{e.name()} == "Options");
  CHECK(std::string_view{e.enumName()} == "Option");
  CHECK(e.isFlag());
  CHECK(not e.isScoped());
  CHECK(e.metaType() == QMetaType::fromType<palette::Options>());

  REQUIRE(e.keyCount() == 3);
  CHECK(std::string_view{e.key(0)} == "NoOption");
  CHECK(std::string_view{e.key(1)} == "First");
  CHECK(std::string_view{e.key(2)} == "Second");
  CHECK(e.value(2) == 2);
}

TEST_CASE("a 64-bit enumeration keeps its full values")
{
  const QMetaObject& mo = palette::staticMetaObject;
  const QMetaEnum    e  = mo.enumerator(index_of_enum(mo, "Wide"));

  REQUIRE(e.isValid());
  CHECK(e.isScoped());
  REQUIRE(e.keyCount() == 2);
  CHECK(e.value64(0) == 1);
  CHECK(e.value64(1) == (1LL << 40));
}

TEST_CASE("every enumerator round-trips through keyToValue and valueToKey")
{
  const QMetaObject& mo = palette::staticMetaObject;

  for(int i = 0; i < mo.enumeratorCount(); ++i)
  {
    const QMetaEnum e = mo.enumerator(i);
    for(int k = 0; k < e.keyCount(); ++k)
    {
      const std::optional<quint64> value = e.value64(k);
      REQUIRE(value.has_value());
      CHECK(e.keyToValue64(e.key(k)) == value);
      CHECK(std::string_view{e.valueToKey(*value)} == std::string_view{e.key(k)});
    }
  }

  const QMetaEnum color = mo.enumerator(index_of_enum(mo, "Color"));
  bool            ok    = false;
  CHECK(color.keyToValue("Green", &ok) == 5);
  CHECK(ok);
  CHECK(std::string_view{color.valueToKey(6)} == "Blue");
  CHECK(color.keyToValue("Purple", &ok) == -1);
  CHECK(not ok);

  const QMetaEnum options = mo.enumerator(index_of_enum(mo, "Options"));
  CHECK(options.keysToValue("First|Second") == 3);
  CHECK(options.valueToKeys(3) == QByteArray{"First|Second"});
}

TEST_CASE("a nested class or type alias is not mistaken for an enumeration")
{
  CHECK(signal_free::staticMetaObject.enumeratorCount() == 0);
  CHECK(signal_free::staticMetaObject.propertyCount() == 1);
}
