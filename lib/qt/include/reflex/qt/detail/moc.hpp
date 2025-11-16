#pragma once

#include <reflex/qt/dump.hpp>
#include <reflex/qt/format.hpp>
#include <reflex/qt/object.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>

#undef signals
#undef slots

namespace reflex::qt::moc
{
namespace fs = std::filesystem;

struct named_value
{
  std::string name;
  std::string value;
};
struct property_meta
{
  bool                       constant;
  bool                       designable;
  bool                       final;
  int                        index;
  std::string                name;
  bool                       required;
  bool                       scriptable;
  bool                       stored;
  std::string                type;
  bool                       user;
  std::optional<std::string> read;
  std::optional<std::string> write;
  std::optional<std::string> notify;
  std::optional<std::string> member;
};
struct meta_argument
{
  std::optional<std::string> name;
  std::string                type;
};
struct method_meta
{
  std::string                access;
  std::string                name;
  std::string                returnType;
  std::vector<meta_argument> arguments;
  int                        index;
};
struct superclass_meta
{
  std::string access;
  std::string name;
};
struct enums_meta
{
  bool                     isClass    = false;
  bool                     isFlag     = false;
  int                      lineNumber = 0;
  std::string              name;
  std::vector<std::string> values;
};
struct class_meta
{
  std::vector<named_value>     classInfos;
  std::string                  className;
  int                          lineNumber;
  std::optional<bool>          object;
  std::optional<bool>          gadget;
  std::vector<property_meta>   properties;
  std::string                  qualifiedClassName;
  std::vector<method_meta>     signals;
  std::vector<method_meta>     slots;
  std::vector<method_meta>     methods;
  std::vector<enums_meta>      enums;
  std::vector<superclass_meta> superClasses;
};
struct filemeta_data
{
  std::vector<class_meta> classes;
  std::string             inputFile;
  int                     outputRevision = 0;
};

std::string dump(std::vector<filemeta_data> const&);

template <typename... Types> void export_json(fs::path const& output, std::string_view moduleName)
{
  std::ofstream stream(output.c_str());

  std::map<std::string, filemeta_data> metadata;

  template for(constexpr auto type : {^^Types...})
  {
    using Type                            = typename[:type:];
    static constexpr auto loc             = source_location_of(type);
    static const auto     filepath        = std::filesystem::path(loc.file_name());
    const QMetaObject*    metaObject      = &Type::staticMetaObject;
    auto const* const     superMetaObject = metaObject->superClass();
    const bool            is_object       = superMetaObject != nullptr;
    const bool            is_gadget       = not is_object;

    class_meta infos;
    infos.className          = metaObject->className();
    infos.qualifiedClassName = meta::qualified_display_string_of(type);
    infos.lineNumber         = loc.line();
    if(is_object)
    {
      infos.object = true;
    }
    else
    {
      infos.gadget = true;
    }

    for(int i = metaObject->classInfoOffset(); i < metaObject->classInfoCount(); ++i)
    {
      const QMetaClassInfo classInfo = metaObject->classInfo(i);
      infos.classInfos.emplace_back(classInfo.name(), classInfo.value());
    }

    // --- Properties ---
    for(int i = metaObject->propertyOffset(); i < metaObject->propertyCount(); ++i)
    {
      QMetaProperty property = metaObject->property(i);
      property_meta desc{
          .constant   = false,
          .designable = true,
          .final      = false,
          .index      = i,
          .name       = property.name(),
          .required   = false,
          .scriptable = true,
          .stored     = true,
          .type       = property.typeName(),
          .user       = false,
      };
      if(is_object)
      {
        desc.read   = property.name();
        desc.notify = std::format("{}Changed", property.name());
        desc.write  = std::format("set{}", property.name());
      }
      else
      {
        desc.member = property.name();
      }
      infos.properties.push_back(std::move(desc));
    }

    // --- Enums ---
    for(int i = metaObject->enumeratorOffset(); i < metaObject->enumeratorCount(); ++i)
    {
      QMetaEnum  e = metaObject->enumerator(i);
      enums_meta em;
      em.name = e.name();
      for(int j = 0; j < e.keyCount(); ++j)
      {
        em.values.push_back(e.key(j));
      }
      infos.enums.push_back(std::move(em));
    }

    // --- Signals/Slots/Invocables ---
    for(int i = 0; i < metaObject->methodCount(); ++i)
    {
      QMetaMethod method = metaObject->method(i);

      if(method.enclosingMetaObject() != metaObject)
      {
        continue;
      }

      std::vector<method_meta>* dest = nullptr;
      switch(method.methodType())
      {
        case QMetaMethod::Signal:
          dest = &infos.signals;
          break;
        case QMetaMethod::Slot:
          dest = &infos.slots;
          break;
        case QMetaMethod::Method:
          dest = &infos.methods;
          break;
        case QMetaMethod::Constructor:
        default:
          break;
      }
      auto& minfos = dest->emplace_back();

      minfos.name   = method.name();
      minfos.index  = method.methodIndex();
      minfos.access = [a = method.access()]
      {
        switch(a)
        {
          using enum QMetaMethod::Access;
          case Private:
            return "private";
          case Protected:
            return "protected";
          case Public:
          default:
            return "public";
        }
      }();

      const auto params_types = method.parameterTypes();
      const auto params_names = method.parameterNames();
      for(int p = 0; p < method.parameterCount(); ++p)
      {
        minfos.arguments.emplace_back( //
                                       // std::string{params_names[p].data()},
            std::nullopt,
            std::string{params_types[p].data()});
      }

      minfos.returnType = QMetaType(method.returnType()).name();
    }

    if(superMetaObject)
    {
      infos.superClasses.emplace_back("public", superMetaObject->className());
    }

    auto& filemeta = metadata[filepath.filename()];
    filemeta.classes.push_back(std::move(infos));
  }

  const auto json = dump(std::move(metadata)
                         | std::views::transform(
                             [](auto&& item) mutable
                             {
                               item.second.inputFile = std::move(item.first);
                               return std::move(item.second);
                             })
                         | std::ranges::to<std::vector>());
  std::cout << json << '\n';
  stream << json << '\n';
}
} // namespace reflex::qt::moc
