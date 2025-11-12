#pragma once

#include <reflex/qt/dump.hpp>
#include <reflex/qt/format.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <print>

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
  bool        constant;
  bool        designable;
  bool        final;
  int         index;
  std::string name;
  std::string notify;
  std::string read;
  bool        required;
  bool        scriptable;
  bool        stored;
  std::string type;
  bool        user;
  std::string write;
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
struct class_meta
{
  std::vector<named_value>     classInfos;
  std::string                  className;
  int                          lineNumber;
  bool                         object;
  std::vector<property_meta>   properties;
  std::string                  qualifiedClassName;
  std::vector<method_meta>     signals;
  std::vector<method_meta>     slots;
  std::vector<method_meta>     methods;
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
  if(fs::exists(output))
  {
    fs::create_directory(output);
  }

  std::string   filename = std::format("{}/{}.json", output.c_str(), moduleName);
  std::ofstream stream(filename);

  std::map<std::string, filemeta_data> metadata;

  template for(constexpr auto type : {^^Types...})
  {
    using Type                       = typename[:type:];
    static constexpr auto loc        = source_location_of(type);
    static const auto     filepath   = std::filesystem::path(loc.file_name());
    const QMetaObject*    metaObject = &Type::staticMetaObject;

    class_meta infos;
    infos.className          = metaObject->className();
    infos.qualifiedClassName = metaObject->className(); // TODO resolve qualified name
    infos.lineNumber         = loc.line();
    infos.object             = true;

    for(int i = metaObject->classInfoOffset(); i < metaObject->classInfoCount(); ++i)
    {
      const QMetaClassInfo classInfo = metaObject->classInfo(i);
      infos.classInfos.emplace_back(classInfo.name(), classInfo.value());
    }

    // --- Properties ---
    for(int i = metaObject->propertyOffset(); i < metaObject->propertyCount(); ++i)
    {
      QMetaProperty property = metaObject->property(i);
      infos.properties.push_back(property_meta{
          .constant   = false,
          .designable = true,
          .final      = false,
          .index      = i,
          .name       = property.name(),
          .notify     = std::format("{}Changed", property.name()),
          .read       = property.name(),
          .required   = false,
          .scriptable = true,
          .stored     = true,
          .type       = property.typeName(),
          .user       = false,
          .write      = std::format("set{}", property.name()),
      });
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

    auto const* const superMetaObject = metaObject->superClass();
    infos.superClasses.emplace_back("public", superMetaObject->className());

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
