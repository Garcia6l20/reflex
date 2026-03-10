export module reflex.core:concepts;

import std;

export namespace reflex
{

template <typename E>
concept enum_c = std::is_enum_v<E>;

template <typename T>
concept aggregate_c = std::meta::is_aggregate_type(^^T) and not std::meta::is_array_type(^^T);

static_assert(not aggregate_c<char[2]>);

template <typename T, typename U>
concept decays_to_c = std::same_as<std::decay_t<T>, U>;

} // namespace reflex
