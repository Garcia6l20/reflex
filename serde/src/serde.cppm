export module reflex.serde;

export import :annotations;
export import :object_visit;
export import :poly;

export import reflex.core;
export import reflex.poly;

/// serializers registry
export namespace reflex::serde::ser
{}

/// deserializers registry
export namespace reflex::serde::de
{}

export namespace reflex::serde
{

consteval auto serializers()
{
  constexpr auto ctx = std::meta::access_context::current();
  return members_of(^^reflex::serde::ser, ctx);
}

constexpr auto with_serializer(std::string_view file_format, auto&& fn)
{
  template for(constexpr auto entry : define_static_array(serializers()))
  {
    if(identifier_of(entry) == file_format)
    {
      using T = typename[:entry:];
      fn(T{});
      return;
    }
    // std::println("Checked serializer: {}", identifier_of(entry));
  }
  throw runtime_error("No serializer found for file format '{}'", file_format);
}

consteval auto deserializers()
{
  constexpr auto ctx = std::meta::access_context::current();
  return members_of(^^reflex::serde::de, ctx);
}

constexpr auto with_deserializer(std::string_view file_format, auto&& fn)
{
  template for(constexpr auto entry : define_static_array(deserializers()))
  {
    if(identifier_of(entry) == file_format)
    {
      using T = typename[:entry:];
      fn(T{});
      return;
    }
    // std::println("Checked deserializer: {}", identifier_of(entry));
  }
  throw runtime_error("No deserializer found for file format '{}'", file_format);
}

} // namespace reflex::serde
