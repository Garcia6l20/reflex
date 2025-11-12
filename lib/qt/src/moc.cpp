#include <reflex/qt/detail/moc.hpp>
#include <reflex/serialize/json.hpp>

namespace reflex::qt::moc
{
std::string dump(std::vector<filemeta_data> const& data)
{
  return serialize::json::dump(data);
}
} // namespace reflex::qt::moc