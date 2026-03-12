export module reflex.core:tu;

import :const_assert;
import std;

export namespace reflex::meta::tu
{
struct unique : std::source_location
{
  consteval unique(std::source_location loc = std::source_location::current())
      : std::source_location(loc)
  {
  }
};

template <template <auto...> typename scope> struct counter
{
  static consteval int value()
  {
    int ii = 0;
    while(is_complete_type(substitute(^^scope,
                                      {
                                          std::meta::reflect_constant(ii)})))
    {
      ++ii;
    }
    return ii;
  }

  static consteval void increment()
  {
    define_aggregate(substitute(^^scope,
                                {
                                    std::meta::reflect_constant(value())}),
                     {});
  }
};

template <template <auto...> typename scope, bool is_unique = true> struct type_registry
{
  static consteval int size(unique u = {})
  {
    int ii = 0;
    while(is_complete_type(substitute(^^scope,
                                      {
                                          std::meta::reflect_constant(ii)})))
    {
      ++ii;
    }
    return ii;
  }

  static consteval auto push(std::meta::info i, unique u = {})
  {
    const_assert(is_type(i), "expected a type", u);

    if constexpr(is_unique)
    {
      if(contains(i, u))
      {
        return;
      }
    }

    auto impl = substitute(^^scope,
                           {
                               std::meta::reflect_constant(size(u))});
    define_aggregate(impl, {data_member_spec(i, {.name = "value"})});
  }

  static consteval auto all(unique u = {})
  {
    auto n = size(u);
    return std::views::iota(0, n)
         | std::views::transform(
               [](int ii)
               {
                 auto impl = substitute(^^scope,
                                        {
                                            std::meta::reflect_constant(ii)});
                 return type_of(
                     nonstatic_data_members_of(impl, std::meta::access_context::unchecked())
                         .front());
               });
  }

  static consteval bool contains(std::meta::info i, unique u = {})
  {
    return std::ranges::contains(all(u), i);
  }

  static consteval auto at(std::size_t index, unique u = {})
  {
    return all(u)[index];
  }
};

} // namespace reflex::meta::tu