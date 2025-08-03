# reflex.testing

Testing framework for c++26.

## Quick example

```cpp
#include <reflex/testing_main.hpp>

// suite namespace must contain "test"
namespace my_test_suite {

  // case function must contain "test"
  void my_first_test() {
    CHECK_THAT(true) == true; // pass
    ASSERT_THAT(true) == true; // pass
    CHECK_THAT(true) == false; // fails (execution continues)
    CHECK_THAT(true) == false; // fails (execution stopped)
    std::unreachable();
  }
}
```
