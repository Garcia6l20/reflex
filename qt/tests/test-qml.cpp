#include "moc-qml-mirror.hpp"

#include <doctest/doctest.h>

#include <reflex/const_check.hpp>
#include <reflex/qt/moc/export.hpp>
#include <reflex/qt/qml.hpp>

#include <QtCore/QMetaClassInfo>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTypeRevision>

#include <string>
#include <string_view>
#include <vector>

namespace qt  = reflex::qt;
namespace moc = reflex::qt::moc;

struct [[= qt::qml{}]] plain : qt::object<plain>
{
  [[= qt::prop{}]] int value = 0;
};

struct [[= qt::qml{.name = "Gauge"}]] named : qt::object<named>
{
};

struct [[= qt::qml{.singleton = true}]] lone : qt::object<lone>
{
};

struct [[= qt::qml{.uncreatable = "ask the factory"}]] sealed : qt::object<sealed>
{
};

struct [[= qt::qml{.name = "span", .added_in = {2, 3}, .removed_in = {3, 0}}]] versioned
    : qt::gadget<versioned>
{
};

struct [[= qt::qml{}]] [[= qt::classinfo{"author", "reflex"}]] mixed : qt::object<mixed>
{
};

struct bare : qt::object<bare>
{
};

struct [[= qt::qml{}]] [[= qt::qml{.singleton = true}]] twice : qt::object<twice>
{
};

struct [[= qt::qml{.name = ""}]] nameless : qt::object<nameless>
{
};

REFLEX_QT_MODULE(qml_types, m)
{
  m.expose<plain>();
  m.expose<lone>();
}

static auto class_infos_of(QMetaObject const& meta) -> std::vector<std::pair<std::string, std::string>>
{
  std::vector<std::pair<std::string, std::string>> found;
  for(int i = meta.classInfoOffset(); i < meta.classInfoCount(); ++i)
  {
    found.emplace_back(meta.classInfo(i).name(), meta.classInfo(i).value());
  }
  return found;
}

static auto class_infos_of(moc::class_meta const& described)
    -> std::vector<std::pair<std::string, std::string>>
{
  std::vector<std::pair<std::string, std::string>> found;
  for(auto const& entry : described.classInfos)
  {
    found.emplace_back(entry.name, entry.value);
  }
  return found;
}

using infos = std::vector<std::pair<std::string, std::string>>;

TEST_CASE("a bare qml annotation publishes QML.Element auto")
{
  const auto expected = class_infos_of(qml_plain::staticMetaObject);

  CHECK(expected == infos{{"QML.Element", "auto"}});
  CHECK(class_infos_of(plain::staticMetaObject) == expected);
  CHECK(class_infos_of(moc::describe<plain>()) == expected);
}

TEST_CASE("a named qml annotation publishes the name")
{
  const auto expected = class_infos_of(qml_named::staticMetaObject);

  CHECK(expected == infos{{"QML.Element", "Gauge"}});
  CHECK(class_infos_of(named::staticMetaObject) == expected);
  CHECK(class_infos_of(moc::describe<named>()) == expected);
}

TEST_CASE("a singleton publishes QML.Singleton after its element")
{
  const auto expected = class_infos_of(qml_lone::staticMetaObject);

  CHECK(expected == infos{{"QML.Element", "auto"}, {"QML.Singleton", "true"}});
  CHECK(class_infos_of(lone::staticMetaObject) == expected);
  CHECK(class_infos_of(moc::describe<lone>()) == expected);
}

TEST_CASE("an uncreatable type publishes its reason next to QML.Creatable")
{
  const auto expected = class_infos_of(qml_sealed::staticMetaObject);

  CHECK(expected
        == infos{{"QML.Element", "auto"},
                 {"QML.Creatable", "false"},
                 {"QML.UncreatableReason", "ask the factory"}});
  CHECK(class_infos_of(sealed::staticMetaObject) == expected);
  CHECK(class_infos_of(moc::describe<sealed>()) == expected);
}

TEST_CASE("a version is encoded the way moc encodes one")
{
  const auto expected = class_infos_of(qml_versioned::staticMetaObject);

  CHECK(expected
        == infos{{"QML.Element", "span"},
                 {"QML.AddedInVersion",
                  std::to_string(QTypeRevision::fromVersion(2, 3).toEncodedVersion<quint16>())},
                 {"QML.RemovedInVersion",
                  std::to_string(QTypeRevision::fromVersion(3, 0).toEncodedVersion<quint16>())}});
  CHECK(class_infos_of(versioned::staticMetaObject) == expected);
  CHECK(class_infos_of(moc::describe<versioned>()) == expected);
}

TEST_CASE("qml class infos come before the classinfo annotations")
{
  const auto expected = class_infos_of(qml_mixed::staticMetaObject);

  CHECK(expected == infos{{"QML.Element", "auto"}, {"author", "reflex"}});
  CHECK(class_infos_of(mixed::staticMetaObject) == expected);
  CHECK(class_infos_of(moc::describe<mixed>()) == expected);
}

TEST_CASE("a class carrying no qml annotation publishes no class info")
{
  CHECK(class_infos_of(qml_none::staticMetaObject).empty());
  CHECK(class_infos_of(bare::staticMetaObject).empty());
  CHECK(class_infos_of(moc::describe<bare>()).empty());
}

consteval
{
  REFLEX_CONSTEVAL_NOTHROW(qt::detail::validate_qml(^^plain));
  REFLEX_CONSTEVAL_THROWS(qt::detail::validate_qml(^^twice));
  REFLEX_CONSTEVAL_THROWS(qt::detail::validate_qml(^^nameless));
}

static_assert(QQmlPrivate::QmlSingleton<lone>::Value);
static_assert(not QQmlPrivate::QmlSingleton<plain>::Value);
static_assert(not QQmlPrivate::QmlSingleton<QObject>::Value);

static_assert(QQmlPrivate::QmlUncreatable<sealed>::Value);
static_assert(not QQmlPrivate::QmlUncreatable<plain>::Value);
static_assert(not QQmlPrivate::QmlUncreatable<QObject>::Value);

TEST_CASE("a module body exposes a qml class like any other")
{
  const auto files = moc::metatypes_of<qml_types>();

  REQUIRE(files.size() == 1);
  REQUIRE(files.front().classes.size() == 2);
  CHECK(files.front().classes[0].className == "plain");
  CHECK(files.front().classes[1].className == "lone");
}
