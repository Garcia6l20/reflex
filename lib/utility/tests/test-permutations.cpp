#include <reflex/views/permutations.hpp>

#include <reflex/testing_main.hpp>

using namespace reflex;

namespace permutations_tests
{
void test12()
{
  {
    static constexpr auto v = std::array{0, 1};
    std::println(" ===== permutations<2> =====");
    constexpr auto p2 = define_static_array(v | views::permutations<2>);
    template for(constexpr auto p : p2)
    {
      std::println(" - {}", p);
    }
    CHECK_CONTEXT(p2);
    STATIC_CHECK_THAT(p2.size() == 2);
    STATIC_CHECK_THAT(p2[0] == std::array{0, 1});
    STATIC_CHECK_THAT(p2[1] == std::array{1, 0});
  }
  {
    static constexpr auto v = std::array{0, 1, 2};
    std::println(" ===== permutations<3> =====");
    constexpr auto p3 = define_static_array(v | views::permutations<3>);
    template for(constexpr auto p : p3)
    {
      std::println(" - {}", p);
    }
    CHECK_CONTEXT(p3);
    STATIC_CHECK_THAT(p3.size() == 6);
    STATIC_CHECK_THAT(p3[0] == std::array{0, 1, 2});
    STATIC_CHECK_THAT(p3[1] == std::array{0, 2, 1});
    STATIC_CHECK_THAT(p3[2] == std::array{1, 0, 2});
    STATIC_CHECK_THAT(p3[3] == std::array{1, 2, 0});
    STATIC_CHECK_THAT(p3[4] == std::array{2, 0, 1});
    STATIC_CHECK_THAT(p3[5] == std::array{2, 1, 0});
  }
}
} // namespace permutations_tests
