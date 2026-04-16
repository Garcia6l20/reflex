#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace reflex::testutils
{
bool set_env(const char* name, const char* value, int overwrite)
{
#ifdef _WIN32
  // if overwrite is 0 and the variable already exists do nothing
  if(!overwrite)
  {
    // GetEnvironmentVariableA returns the required buffer size excluding null
    // or 0 if the variable does not exist.
    DWORD ret = GetEnvironmentVariableA(name, nullptr, 0);
    if(ret != 0)
    {
      // variable exists -> success without changing it
      return true;
    }
  }
  // set or overwrite the variable
  if(SetEnvironmentVariableA(name, value) == 0)
  {
    return false;
  }
  return true;
#else
  if(::setenv(name, value, overwrite) != 0)
  {
    return false;
  }
  return true;
#endif
}
bool unset_env(const char* name)
{
#ifdef _WIN32
  // passing NULL as the value deletes the environment variable
  if(SetEnvironmentVariableA(name, nullptr) == 0)
  {
    return false;
  }
  return true;
#else
  if(::unsetenv(name) != 0)
  {
    return false;
  }
  return true;
#endif
}
} // namespace reflex::testutils