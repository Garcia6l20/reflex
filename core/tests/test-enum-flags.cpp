#include <doctest/doctest.h>

import reflex.core;

using namespace reflex;

enum class [[= reflex::derive{EnumFlags, Parse, Format}]] FilePermissions
{
  None    = 0,
  Read    = 1 << 0,
  Write   = 1 << 1,
  Execute = 1 << 2
};

static_assert(derives(^^FilePermissions, EnumFlags));
static_assert(derives(^^FilePermissions, Parse));
static_assert(derives(^^FilePermissions, Format));

TEST_CASE("reflex::core::enum_flags: FilePermissions")
{
  FilePermissions perms = FilePermissions::Read | FilePermissions::Write;
  CHECK((perms & FilePermissions::Read) == FilePermissions::Read);
  CHECK((perms & FilePermissions::Write) == FilePermissions::Write);
  CHECK((perms & FilePermissions::Execute) == FilePermissions::None);
  auto formatted = std::format("{}", perms);
  CHECK_EQ(formatted, "Read|Write");
  auto parsed = parse<FilePermissions>(formatted);
  CHECK(parsed);
  CHECK_EQ(*parsed, perms);
}

extern "C" {
    typedef enum c_file_permissions
    {
        c_file_none    = 0,
        c_file_read    = 1 << 0,
        c_file_write   = 1 << 1,
        c_file_execute = 1 << 2
    } c_file_permissions;
}

// apply derive for external code

namespace reflex {
    template <>
    inline constexpr bool derives_v<c_file_permissions, EnumFlags> = true;

    template <>
    inline constexpr bool derives_v<c_file_permissions, Parse> = true;

    template <>
    inline constexpr bool derives_v<c_file_permissions, Format> = true;
}


TEST_CASE("reflex::core::enum_flags: C enums")
{
  c_file_permissions perms = c_file_read | c_file_write;
  CHECK((perms & c_file_read) == c_file_read);
  CHECK((perms & c_file_write) == c_file_write);
  CHECK((perms & c_file_execute) == c_file_none);
  auto formatted = std::format("{:-7}", perms);
  CHECK_EQ(formatted, "read|write");
  auto parsed = parse<c_file_permissions, "-7">(formatted);
  CHECK(parsed);
  CHECK_EQ(*parsed, perms);
}
