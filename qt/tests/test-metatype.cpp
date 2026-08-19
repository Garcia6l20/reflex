#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QMetaType>
#include <QtCore/QVariant>

#include <string_view>

/// Nothing in this file may name `staticMetaObject` before the metatype cases
/// below: the cycle they guard against only fires when the metatype entry point
/// is the first thing in the translation unit to instantiate it.

namespace qt = reflex::qt;

namespace metatype_test
{
struct boxed : qt::gadget<boxed>
{
  [[= qt::prop{}]] int x = 0;
};

struct registered : qt::gadget<registered>
{
  [[= qt::prop{}]] int x = 0;
};

struct typed : qt::gadget<typed>
{
  [[= qt::prop{}]] int x = 0;
};

namespace inner
{
struct deep : qt::gadget<deep>
{
  [[= qt::prop{}]] int x = 0;
};
}

struct host
{
  struct nested : qt::gadget<nested>
  {
    [[= qt::prop{}]] int x = 0;
  };
};

struct base : qt::gadget<base>
{
  [[= qt::prop{}]] int x = 0;
};

struct derived : base
{
  [[= qt::prop{}]] int y = 0;
};
}

TEST_CASE("a gadget boxes into a QVariant with no metaobject warm-up")
{
  metatype_test::boxed value;
  value.x = 7;

  const QVariant boxed = QVariant::fromValue(value);

  REQUIRE(boxed.isValid());
  CHECK(std::string_view{boxed.metaType().name()} == "metatype_test::boxed");
  CHECK(boxed.metaType().flags().testFlag(QMetaType::IsGadget));
  CHECK(boxed.value<metatype_test::boxed>().x == 7);
}

TEST_CASE("a gadget registers with no metaobject warm-up")
{
  const int id = qRegisterMetaType<metatype_test::registered>();

  CHECK(id != QMetaType::UnknownType);
  CHECK(QMetaType::fromName("metatype_test::registered").id() == id);
  CHECK(QMetaType{id}.flags().testFlag(QMetaType::IsGadget));
}

TEST_CASE("QMetaType::fromType answers with no metaobject warm-up")
{
  const QMetaType type = QMetaType::fromType<metatype_test::typed>();

  CHECK(std::string_view{type.name()} == "metatype_test::typed");
  CHECK(type.flags().testFlag(QMetaType::IsGadget));
}

TEST_CASE("the registered name is the qualified type name")
{
  const int deep_id   = qRegisterMetaType<metatype_test::inner::deep>();
  const int nested_id = qRegisterMetaType<metatype_test::host::nested>();

  CHECK(std::string_view{QMetaType{deep_id}.name()} == "metatype_test::inner::deep");
  CHECK(std::string_view{QMetaType{nested_id}.name()} == "metatype_test::host::nested");
  CHECK(QMetaType::fromName("metatype_test::inner::deep").id() == deep_id);
  CHECK(QMetaType::fromName("metatype_test::host::nested").id() == nested_id);
}

TEST_CASE("a gadget inheriting a gadget keeps its own metatype")
{
  const int derived_id = qRegisterMetaType<metatype_test::derived>();
  const int base_id    = qRegisterMetaType<metatype_test::base>();

  CHECK(derived_id != base_id);
  CHECK(std::string_view{QMetaType{derived_id}.name()} == "metatype_test::derived");
  CHECK(std::string_view{QMetaType{base_id}.name()} == "metatype_test::base");
  CHECK(QMetaType::fromName("metatype_test::base").id() == base_id);
}
