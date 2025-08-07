#include <print>
#include <reflex/match_patten.hpp>
#include <reflex/regitry.hpp>

using namespace reflex;

struct __awesome_plugins_scope
{
};

struct awesome_plugin_registry : meta::registry<__awesome_plugins_scope>
{
  template <auto defer = [] {}> static consteval auto make_variant_type()
  {
    std::vector<meta::info> types{^^std::monostate};
    types.append_range(awesome_plugin_registry::all<defer>());
    return substitute(^^std::variant, types);
  }

  template <auto defer = [] {}> static auto make(std::string_view str) -> typename[:make_variant_type<defer>():]
  {
    template for(constexpr auto t : awesome_plugin_registry::all<defer>())
    {
      if(display_string_of(t) == str)
      {
        return typename[:t:]{};
      }
    }
    return {};
  }
};

template <typename T> struct awesome_plugin : awesome_plugin_registry::auto_<T>
{
};

struct plugin1 : awesome_plugin<plugin1>
{
  auto say_your_name() const
  {
    return "plugin1";
  }
};
struct plugin2 : awesome_plugin<plugin2>
{
  auto say_your_name() const
  {
    return "your name";
  }
};
struct plugin3 : awesome_plugin<plugin3>
{
  auto say_your_name() const
  {
    return "Heisenberg";
  }
};

int main(int argc, const char** argv)
{
  static_assert(awesome_plugin_registry::size() == 3);
  static_assert(awesome_plugin_registry::contains<plugin1>());
  static_assert(awesome_plugin_registry::contains<plugin2>());
  static_assert(awesome_plugin_registry::contains<plugin3>());

  if(argc > 1)
  {
    std::string_view requested = argv[1];
    auto             var       = awesome_plugin_registry::make(requested);
    std::visit(
        match_pattern{
            []<typename Plugin>(Plugin const& plugin)
              requires(awesome_plugin_registry::contains<Plugin>())
            { std::println("plugin name is: {}", plugin.say_your_name()); },
            [&]<typename NotAPlugin>(NotAPlugin const& not_a_plugin)
            { std::println("{} is not a plugin, {} requested", display_string_of(^^NotAPlugin), requested); }},
            var);
  }
  else
  {
    std::println("available plugins:");
    template for(constexpr auto t : awesome_plugin_registry::all())
    {
      std::println(" - {}", display_string_of(t));
    }
  }
  return 0;
}
