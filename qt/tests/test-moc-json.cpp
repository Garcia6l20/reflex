#include "moc-mirror.hpp"
#include "moc-pair.hpp"

#include <reflex/qt/moc/export.hpp>

#include <doctest/doctest.h>

#include <QtCore/QMetaClassInfo>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>

#include <filesystem>
#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace moc = reflex::qt::moc;
namespace qt  = reflex::qt;

struct dot : qt::gadget<dot>
{
  [[= qt::prop{}]] int x = 0;

  [[= qt::invocable]] int twice() const
  {
    return 2 * x;
  }
};

struct palette : qt::object<palette>
{
  enum class Mode
  {
    Fast,
    Slow
  };

  enum Option
  {
    NoOption = 0,
    First    = 1,
    Second   = 2
  };

  using Options = QFlags<Option>;

  [[= qt::prop{}]] Options opts;
};

REFLEX_QT_MODULE(pair_types, m)
{
  m.expose<twin>();
}

REFLEX_QT_MODULE(empty_types, m)
{
  (void)m;
}

TEST_CASE("an empty module produces a valid document with no class in it")
{
  CHECK(moc::metatypes_of<empty_types>().empty());
  CHECK_EQ(moc::format_metatypes<empty_types>(), "[]");
}

TEST_CASE("the document names the header, the schema revision and one entry per header")
{
  const auto document = moc::metatypes_of<pair_types>();

  REQUIRE_EQ(document.size(), 1u);
  CHECK_EQ(document.front().outputRevision, 69);
  CHECK_EQ(document.front().outputRevision, moc::output_revision);
  CHECK(std::string_view{document.front().inputFile}.ends_with("moc-pair.hpp"));
  REQUIRE_EQ(document.front().classes.size(), 1u);
}

TEST_CASE("include_roots shortens inputFile to what qmltyperegistrar can include")
{
  const auto here = std::filesystem::weakly_canonical(
                        std::filesystem::path{std::source_location::current().file_name()})
                        .parent_path();
  const auto full = moc::metatypes_of<pair_types>().front().inputFile;

  CHECK_EQ(full, (here / "moc-pair.hpp").generic_string());
  CHECK_EQ(moc::metatypes_of<pair_types>({{here}}).front().inputFile, "moc-pair.hpp");
  CHECK_EQ(moc::metatypes_of<pair_types>({{here.parent_path()}}).front().inputFile,
           "tests/moc-pair.hpp");
  CHECK_EQ(moc::metatypes_of<pair_types>({{here.parent_path().parent_path()}}).front().inputFile,
           "qt/tests/moc-pair.hpp");
  CHECK_EQ(moc::metatypes_of<pair_types>({{"/nowhere"}}).front().inputFile, full);
}

TEST_CASE("a class describes itself the way moc describes its Q_OBJECT mirror")
{
  const auto described = moc::describe<twin>();

  CHECK_EQ(described.className, "twin");
  CHECK_EQ(described.qualifiedClassName, "twin");
  CHECK(described.object.has_value());
  CHECK_FALSE(described.gadget.has_value());

  REQUIRE_EQ(described.superClasses.size(), 1u);
  CHECK_EQ(described.superClasses.front().access, "public");
  CHECK_EQ(described.superClasses.front().name, "QObject");

  REQUIRE_EQ(described.classInfos.size(), 2u);
  CHECK_EQ(described.classInfos[0].name, "QML.Element");
  CHECK_EQ(described.classInfos[0].value, "auto");
  CHECK_EQ(described.classInfos[1].name, "author");
  CHECK_EQ(described.classInfos[1].value, "reflex");

  REQUIRE_EQ(described.enums.size(), 2u);
  CHECK_EQ(described.enums.front().name, "Mode");
  CHECK_FALSE(described.enums.front().isFlag);
  CHECK_FALSE(described.enums.front().isClass);
  CHECK_FALSE(described.enums.front().alias.has_value());
  CHECK_FALSE(described.enums.front().type.has_value());
  CHECK_EQ(described.enums.front().values, std::vector<std::string>{"Fast", "Slow"});

  CHECK_EQ(described.enums.back().name, "Level");
  CHECK(described.enums.back().isClass);
  CHECK_EQ(described.enums.back().type, "uchar");

  REQUIRE_EQ(described.properties.size(), 6u);

  SUBCASE("an accessor pair is read and write, never member")
  {
    auto const& count = described.properties[0];
    CHECK_EQ(count.name, "count");
    CHECK_EQ(count.index, 0);
    CHECK_EQ(count.type, "int");
    CHECK_EQ(count.read, "getCount");
    CHECK_EQ(count.write, "setCount");
    CHECK_EQ(count.notify, "countChanged");
    CHECK_FALSE(count.member.has_value());
  }

  SUBCASE("a bare property is member, since a member is what Qt writes through")
  {
    auto const& extra = described.properties[1];
    CHECK_EQ(extra.name, "extra");
    CHECK_EQ(extra.member, "extra");
    CHECK_EQ(extra.notify, "extraChanged");
    CHECK_FALSE(extra.read.has_value());
    CHECK_FALSE(extra.write.has_value());
  }

  SUBCASE("a constant property neither writes nor notifies")
  {
    auto const& title = described.properties[2];
    CHECK_EQ(title.name, "title");
    CHECK_EQ(title.type, "QString");
    CHECK(title.constant);
    CHECK_EQ(title.read, "getTitle");
    CHECK_FALSE(title.member.has_value());
    CHECK_FALSE(title.write.has_value());
    CHECK_FALSE(title.notify.has_value());
  }

  SUBCASE("an enum property carries the qualified type name, and its own flags")
  {
    auto const& mode = described.properties[3];
    CHECK_EQ(mode.name, "mode");
    CHECK_EQ(mode.type, "twin::Mode");
    CHECK(mode.final);
    CHECK(mode.required);
    CHECK_FALSE(mode.constant);
    CHECK_EQ(mode.member, "mode");
  }

  SUBCASE("a constant property with no getter still names the member it reads")
  {
    auto const& fixed = described.properties[4];
    CHECK_EQ(fixed.name, "fixed");
    CHECK(fixed.constant);
    CHECK_EQ(fixed.member, "fixed");
    CHECK_FALSE(fixed.read.has_value());
    CHECK_FALSE(fixed.write.has_value());
    CHECK_FALSE(fixed.notify.has_value());
  }

  SUBCASE("a QFlags alias the class does not declare keeps its own name")
  {
    auto const& align = described.properties[5];
    CHECK_EQ(align.name, "align");
    CHECK_EQ(align.type, "Qt::Alignment");
    CHECK_EQ(align.member, "align");
    CHECK_EQ(align.notify, "alignChanged");
  }

  SUBCASE("the method indices are the metaobject's own, notifiers included")
  {
    REQUIRE_EQ(described.signal_methods.size(), 6u);
    CHECK_EQ(described.signal_methods[0].name, "bumped");
    CHECK_EQ(described.signal_methods[0].index, 0);
    CHECK_EQ(described.signal_methods[1].name, "nudged");
    CHECK_EQ(described.signal_methods[2].name, "countChanged");
    CHECK_EQ(described.signal_methods[3].name, "extraChanged");
    CHECK_EQ(described.signal_methods[4].name, "modeChanged");
    CHECK_EQ(described.signal_methods[4].index, 4);
    CHECK_EQ(described.signal_methods[5].name, "alignChanged");

    REQUIRE_EQ(described.slot_methods.size(), 2u);
    CHECK_EQ(described.slot_methods[0].name, "increment");
    CHECK_EQ(described.slot_methods[0].index, 6);
    CHECK_EQ(described.slot_methods[0].access, "public");
    CHECK_EQ(described.slot_methods[1].name, "hidden");
    CHECK_EQ(described.slot_methods[1].access, "private");

    REQUIRE_EQ(described.methods.size(), 2u);
    CHECK_EQ(described.methods[0].name, "caption");
    CHECK_EQ(described.methods[0].index, 8);
    CHECK_EQ(described.methods[0].returnType, "QString");
    CHECK_EQ(described.methods[0].isConst, true);
    CHECK_EQ(described.methods[1].name, "reset");
    CHECK_EQ(described.methods[1].returnType, "void");
    CHECK_FALSE(described.methods[1].isConst.has_value());
  }

  SUBCASE("a private signal is described public, a private slot stays private")
  {
    CHECK_EQ(described.signal_methods[1].name, "nudged");
    CHECK_EQ(described.signal_methods[1].access, "public");
    CHECK_EQ(described.slot_methods[1].name, "hidden");
    CHECK_EQ(described.slot_methods[1].access, "private");
  }

  SUBCASE("a slot parameter keeps its name, a signal argument has none to keep")
  {
    REQUIRE_EQ(described.slot_methods[0].arguments.size(), 1u);
    CHECK_EQ(described.slot_methods[0].arguments.front().name, "by");
    CHECK_EQ(described.slot_methods[0].arguments.front().type, "int");

    REQUIRE_EQ(described.signal_methods[0].arguments.size(), 1u);
    CHECK_FALSE(described.signal_methods[0].arguments.front().name.has_value());
    CHECK_EQ(described.signal_methods[0].arguments.front().type, "int");
  }
}

TEST_CASE("an absent field is left out of the JSON rather than written as null")
{
  const auto json = moc::format_metatypes<pair_types>();

  CHECK_EQ(json.find("null"), std::string::npos);
  CHECK_EQ(json.find("\"arguments\":[]"), std::string::npos);
  CHECK_EQ(json.find("\"enums\":[]"), std::string::npos);
  CHECK_EQ(json.find("\"gadget\""), std::string::npos);
  CHECK_NE(json.find("\"object\":true"), std::string::npos);
  CHECK_NE(json.find("\"outputRevision\":69"), std::string::npos);
}

TEST_CASE("the field names moc's schema reserves survive the C++ spelling")
{
  const auto json = moc::format_metatypes<pair_types>();

  CHECK_NE(json.find("\"signals\":["), std::string::npos);
  CHECK_NE(json.find("\"slots\":["), std::string::npos);
  CHECK_NE(json.find("\"virtual\":false"), std::string::npos);
  CHECK_NE(json.find("\"override\":false"), std::string::npos);
  CHECK_EQ(json.find("signal_methods"), std::string::npos);
  CHECK_EQ(json.find("virtual_"), std::string::npos);
}

TEST_CASE("a gadget says gadget, and names no superclass")
{
  const auto described = moc::describe<dot>();

  CHECK(described.gadget.has_value());
  CHECK_FALSE(described.object.has_value());
  CHECK(described.superClasses.empty());
  CHECK(described.signal_methods.empty());
  CHECK(described.slot_methods.empty());
  REQUIRE_EQ(described.properties.size(), 1u);
  CHECK_EQ(described.properties.front().member, "x");
  CHECK_FALSE(described.properties.front().notify.has_value());
  REQUIRE_EQ(described.methods.size(), 1u);
  CHECK_EQ(described.methods.front().name, "twice");
  CHECK_EQ(described.methods.front().returnType, "int");
}

TEST_CASE("a flags alias is described by its own name and the enumeration it aliases")
{
  const auto described = moc::describe<palette>();

  REQUIRE_EQ(described.enums.size(), 3u);

  auto const& scoped = described.enums[0];
  CHECK_EQ(scoped.name, "Mode");
  CHECK(scoped.isClass);
  CHECK_FALSE(scoped.isFlag);

  auto const& plain = described.enums[1];
  CHECK_EQ(plain.name, "Option");
  CHECK_FALSE(plain.isFlag);
  CHECK_FALSE(plain.alias.has_value());

  auto const& flags = described.enums[2];
  CHECK_EQ(flags.name, "Options");
  CHECK(flags.isFlag);
  CHECK_EQ(flags.alias, "Option");
  CHECK_EQ(flags.values, std::vector<std::string>{"NoOption", "First", "Second"});

  REQUIRE_EQ(described.properties.size(), 1u);
  CHECK_EQ(described.properties.front().type, "palette::Options");
}

static auto folded(char const* text) -> std::string
{
  std::string out{text};
  for(std::string_view owner : {"mirror", "twin"})
  {
    for(auto at = out.find(owner); at != std::string::npos; at = out.find(owner))
    {
      out.replace(at, owner.size(), "CLASS");
    }
  }
  return out;
}

static auto class_info_rows(QMetaObject const& meta) -> std::vector<std::string>
{
  std::vector<std::string> rows;
  for(int i = meta.classInfoOffset(); i < meta.classInfoCount(); ++i)
  {
    rows.push_back(std::format("{} = {}", meta.classInfo(i).name(), meta.classInfo(i).value()));
  }
  return rows;
}

static auto method_rows(QMetaObject const& meta) -> std::vector<std::string>
{
  std::vector<std::string> rows;
  for(int i = meta.methodOffset(); i < meta.methodCount(); ++i)
  {
    const QMetaMethod method = meta.method(i);
    rows.push_back(std::format("{} {} {} {}",
                               static_cast<int>(method.methodType()),
                               static_cast<int>(method.access()),
                               folded(method.typeName()),
                               folded(method.methodSignature().constData())));
  }
  return rows;
}

static auto property_rows(QMetaObject const& meta) -> std::vector<std::string>
{
  std::vector<std::string> rows;
  for(int i = meta.propertyOffset(); i < meta.propertyCount(); ++i)
  {
    const QMetaProperty property = meta.property(i);
    rows.push_back(std::format(
        "{} : {} r{} w{} c{} f{} q{} n{} s{} d{} u{} b{}",
        property.name(),
        folded(property.typeName()),
        property.isReadable(),
        property.isWritable(),
        property.isConstant(),
        property.isFinal(),
        property.isRequired(),
        property.hasNotifySignal()
            ? folded(property.notifySignal().methodSignature().constData())
            : std::string{"-"},
        property.isStored(),
        property.isDesignable(),
        property.isUser(),
        property.isBindable()));
  }
  return rows;
}

static auto enum_rows(QMetaObject const& meta) -> std::vector<std::string>
{
  std::vector<std::string> rows;
  for(int i = meta.enumeratorOffset(); i < meta.enumeratorCount(); ++i)
  {
    const QMetaEnum enumerator = meta.enumerator(i);
    std::string     row        = std::format("{} flag{} scoped{} in {}",
                                  enumerator.name(),
                                  enumerator.isFlag(),
                                  enumerator.isScoped(),
                                  folded(enumerator.scope()));
    for(int k = 0; k < enumerator.keyCount(); ++k)
    {
      row += std::format(" {}={}", enumerator.key(k), enumerator.value(k));
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

TEST_CASE("the metaobject reflex builds is the one moc builds for the mirror")
{
  CHECK(class_info_rows(twin::staticMetaObject) == class_info_rows(mirror::staticMetaObject));
  CHECK(method_rows(twin::staticMetaObject) == method_rows(mirror::staticMetaObject));
  CHECK(property_rows(twin::staticMetaObject) == property_rows(mirror::staticMetaObject));
  CHECK(enum_rows(twin::staticMetaObject) == enum_rows(mirror::staticMetaObject));

  CHECK(std::string_view{twin::staticMetaObject.superClass()->className()}
        == mirror::staticMetaObject.superClass()->className());

  CHECK(class_info_rows(twin_qml::staticMetaObject)
        == class_info_rows(mirror_qml::staticMetaObject));
  CHECK(property_rows(twin_qml::staticMetaObject) == property_rows(mirror_qml::staticMetaObject));

  CHECK(class_info_rows(twin_gadget::staticMetaObject)
        == class_info_rows(mirror_gadget::staticMetaObject));
  CHECK(method_rows(twin_gadget::staticMetaObject) == method_rows(mirror_gadget::staticMetaObject));
  CHECK(property_rows(twin_gadget::staticMetaObject)
        == property_rows(mirror_gadget::staticMetaObject));

  CHECK(property_rows(twin_base::staticMetaObject) == property_rows(mirror_base::staticMetaObject));
  CHECK(method_rows(twin_base::staticMetaObject) == method_rows(mirror_base::staticMetaObject));

  CHECK(property_rows(twin_derived::staticMetaObject)
        == property_rows(mirror_derived::staticMetaObject));
  CHECK(method_rows(twin_derived::staticMetaObject)
        == method_rows(mirror_derived::staticMetaObject));
  CHECK(folded(twin_derived::staticMetaObject.superClass()->className())
        == folded(mirror_derived::staticMetaObject.superClass()->className()));

  CHECK(enum_rows(twin_flags::staticMetaObject) == enum_rows(mirror_flags::staticMetaObject));
  CHECK(property_rows(twin_flags::staticMetaObject)
        == property_rows(mirror_flags::staticMetaObject));
  CHECK(method_rows(twin_flags::staticMetaObject) == method_rows(mirror_flags::staticMetaObject));

  CHECK(property_rows(twin_styled::staticMetaObject)
        == property_rows(mirror_styled::staticMetaObject));
  CHECK(method_rows(twin_styled::staticMetaObject) == method_rows(mirror_styled::staticMetaObject));
}
