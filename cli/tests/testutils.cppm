export module reflex.testutils;

export import :pipe_capture;

export namespace reflex::testutils
{
  bool set_env(const char* name, const char* value, int overwrite);
  bool unset_env(const char* name);
} // namespace reflex::testutils
