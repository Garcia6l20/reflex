#pragma once

// Deliberately includes nothing. The macros name only std::meta::exception,
// which any translation unit using them already has, and a header pulling in
// <meta> here would fight `import std;` in a module build.

// Rejects an argument from a consteval function. Unlike static_assert this works
// on a function parameter, and unlike a call to a non-constexpr function it
// carries a message and stays catchable, so a test can pin the rejection.
//
//   consteval rename(std::string_view name) : constant_string{name}
//   {
//     REFLEX_META_CHECK(name.find('.') == npos, "a rename cannot contain a dot", ^^rename);
//   }
#define REFLEX_META_CHECK(cond, message, where)                                                    \
  do                                                                                               \
  {                                                                                                \
    if(not(cond))                                                                                  \
    {                                                                                              \
      throw std::meta::exception((message), (where));                                              \
    }                                                                                              \
  } while(false)

// The two below belong inside a consteval block, where they turn a compile-time
// contract into a test:
//
//   consteval {
//     REFLEX_CONSTEVAL_NOTHROW(rename{"foo"});
//     REFLEX_CONSTEVAL_THROWS(rename{"foo.bar"});
//   }
//
// Variadic so an argument list containing a comma still reaches the macro whole.

// The expression must reject its input.
#define REFLEX_CONSTEVAL_THROWS(...)                                                               \
  do                                                                                               \
  {                                                                                                \
    bool reflex_threw = false;                                                                     \
    try                                                                                            \
    {                                                                                              \
      __VA_ARGS__;                                                                                 \
    }                                                                                              \
    catch(std::meta::exception const&)                                                             \
    {                                                                                              \
      reflex_threw = true;                                                                         \
    }                                                                                              \
    if(not reflex_threw)                                                                           \
    {                                                                                              \
      throw std::meta::exception("expected a rejection from: " #__VA_ARGS__, ^^::);                \
    }                                                                                              \
  } while(false)

// The expression must be accepted. Bare evaluation would already fail the build,
// this names which expression did it.
#define REFLEX_CONSTEVAL_NOTHROW(...)                                                              \
  do                                                                                               \
  {                                                                                                \
    try                                                                                            \
    {                                                                                              \
      __VA_ARGS__;                                                                                 \
    }                                                                                              \
    catch(std::meta::exception const&)                                                             \
    {                                                                                              \
      throw std::meta::exception("unexpected rejection from: " #__VA_ARGS__, ^^::);                \
    }                                                                                              \
  } while(false)

// The expression must reject its input, with a message containing `expected`.
#define REFLEX_CONSTEVAL_THROWS_WITH(expected, ...)                                                \
  do                                                                                               \
  {                                                                                                \
    bool reflex_threw = false;                                                                     \
    bool reflex_found = false;                                                                     \
    try                                                                                            \
    {                                                                                              \
      __VA_ARGS__;                                                                                 \
    }                                                                                              \
    catch(std::meta::exception const& reflex_error)                                                \
    {                                                                                              \
      reflex_threw = true;                                                                         \
      for(const char* at = reflex_error.what(); *at and not reflex_found; ++at)                    \
      {                                                                                            \
        const char* text  = at;                                                                    \
        const char* probe = (expected);                                                            \
        while(*probe and *text == *probe)                                                          \
        {                                                                                          \
          ++text;                                                                                  \
          ++probe;                                                                                 \
        }                                                                                          \
        reflex_found = *probe == '\0';                                                             \
      }                                                                                            \
    }                                                                                              \
    if(not reflex_threw)                                                                           \
    {                                                                                              \
      throw std::meta::exception("expected a rejection from: " #__VA_ARGS__, ^^::);                \
    }                                                                                              \
    if(not reflex_found)                                                                           \
    {                                                                                              \
      throw std::meta::exception("the rejection of " #__VA_ARGS__ " does not say " #expected,      \
                                 ^^::);                                                            \
    }                                                                                              \
  } while(false)
