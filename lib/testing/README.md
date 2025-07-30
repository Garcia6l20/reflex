# reflex.testing

Testing framework for c++26.

## Quick example

```cpp
#include <reflex/testing_main.hpp>

// suite namespace must contain "test"
namespace my_test_suite {

  // case function must contain "test"
  void my_first_test() {
    check_that(true) == true; // pass
    assert_that(true) == true; // pass
    check_that(true) == false; // fails (execution continues)
    check_that(true) == false; // fails (execution stopped)
    std::unreachable();
  }
}
```
