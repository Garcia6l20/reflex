#include <reflex/parse.hpp>

namespace reflex
{

template struct parser<std::int32_t>;
template struct parser<std::int64_t>;
template struct parser<std::uint32_t>;
template struct parser<std::uint64_t>;
template struct parser<float>;
template struct parser<double>;

} // namespace reflex