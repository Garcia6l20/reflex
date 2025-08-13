#include <reflex/views/cartesian_product.hpp>

#include <reflex/testing_main.hpp>

using namespace reflex;

namespace cartesian_product_tests
{
void test12()
{
  static constexpr auto v = std::array{0, 1};
  {
    std::println(" ===== cartesian_product<2> =====");
    constexpr auto p2 = define_static_array(v | views::cartesian_product<2>);
    template for(constexpr auto p : p2)
    {
      std::println(" - {}", p);
    }
    CHECK_CONTEXT(p2);
    STATIC_CHECK_THAT(p2.size() == 4);
    STATIC_CHECK_THAT(p2[0] == std::array{0, 0});
    STATIC_CHECK_THAT(p2[1] == std::array{1, 0});
    STATIC_CHECK_THAT(p2[2] == std::array{0, 1});
    STATIC_CHECK_THAT(p2[3] == std::array{1, 1});
  }
  std::println(" ===== cartesian_product<3> =====");
  for(const auto& p : v | views::cartesian_product<3>)
  {
    std::println(" - {}", p);
  }
  std::println(" ===== cartesian_product<4> =====");
  for(const auto& p : v | views::cartesian_product<4>)
  {
    std::println(" - {}", p);
  }
}
} // namespace permutation_tests
