#include <cstdlib>
namespace reflex::testutils
{
bool set_env(const char* name, const char* value, int overwrite)
{
  if(::setenv(name, value, overwrite) != 0)
  {
    return false;
  }
  return true;
}
bool unset_env(const char* name)
{
  if(::unsetenv(name) != 0)
  {
    return false;
  }
  return true;
}
} // namespace reflex::testutils