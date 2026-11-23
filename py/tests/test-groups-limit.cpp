import reflex.py;
import std;

namespace
{
  struct wide
  {
    auto method_000() const -> int;
    auto method_037() const -> int;
    auto method_074() const -> int;
    auto method_111() const -> int;
    auto method_148() const -> int;
    auto method_035() const -> int;
    auto method_072() const -> int;
    auto method_109() const -> int;
    auto method_146() const -> int;
    auto method_033() const -> int;
    auto method_070() const -> int;
    auto method_107() const -> int;
    auto method_144() const -> int;
    auto method_031() const -> int;
    auto method_068() const -> int;
    auto method_105() const -> int;
    auto method_142() const -> int;
    auto method_029() const -> int;
    auto method_066() const -> int;
    auto method_103() const -> int;
    auto method_140() const -> int;
    auto method_027() const -> int;
    auto method_064() const -> int;
    auto method_101() const -> int;
    auto method_138() const -> int;
    auto method_025() const -> int;
    auto method_062() const -> int;
    auto method_099() const -> int;
    auto method_136() const -> int;
    auto method_023() const -> int;
    auto method_060() const -> int;
    auto method_097() const -> int;
    auto method_134() const -> int;
    auto method_021() const -> int;
    auto method_058() const -> int;
    auto method_095() const -> int;
    auto method_132() const -> int;
    auto method_019() const -> int;
    auto method_056() const -> int;
    auto method_093() const -> int;
    auto method_130() const -> int;
    auto method_017() const -> int;
    auto method_054() const -> int;
    auto method_091() const -> int;
    auto method_128() const -> int;
    auto method_015() const -> int;
    auto method_052() const -> int;
    auto method_089() const -> int;
    auto method_126() const -> int;
    auto method_013() const -> int;
    auto method_050() const -> int;
    auto method_087() const -> int;
    auto method_124() const -> int;
    auto method_011() const -> int;
    auto method_048() const -> int;
    auto method_085() const -> int;
    auto method_122() const -> int;
    auto method_009() const -> int;
    auto method_046() const -> int;
    auto method_083() const -> int;
    auto method_120() const -> int;
    auto method_007() const -> int;
    auto method_044() const -> int;
    auto method_081() const -> int;
    auto method_118() const -> int;
    auto method_005() const -> int;
    auto method_042() const -> int;
    auto method_079() const -> int;
    auto method_116() const -> int;
    auto method_003() const -> int;
    auto method_040() const -> int;
    auto method_077() const -> int;
    auto method_114() const -> int;
    auto method_001() const -> int;
    auto method_038() const -> int;
    auto method_075() const -> int;
    auto method_112() const -> int;
    auto method_149() const -> int;
    auto method_036() const -> int;
    auto method_073() const -> int;
    auto method_110() const -> int;
    auto method_147() const -> int;
    auto method_034() const -> int;
    auto method_071() const -> int;
    auto method_108() const -> int;
    auto method_145() const -> int;
    auto method_032() const -> int;
    auto method_069() const -> int;
    auto method_106() const -> int;
    auto method_143() const -> int;
    auto method_030() const -> int;
    auto method_067() const -> int;
    auto method_104() const -> int;
    auto method_141() const -> int;
    auto method_028() const -> int;
    auto method_065() const -> int;
    auto method_102() const -> int;
    auto method_139() const -> int;
    auto method_026() const -> int;
    auto method_063() const -> int;
    auto method_100() const -> int;
    auto method_137() const -> int;
    auto method_024() const -> int;
    auto method_061() const -> int;
    auto method_098() const -> int;
    auto method_135() const -> int;
    auto method_022() const -> int;
    auto method_059() const -> int;
    auto method_096() const -> int;
    auto method_133() const -> int;
    auto method_020() const -> int;
    auto method_057() const -> int;
    auto method_094() const -> int;
    auto method_131() const -> int;
    auto method_018() const -> int;
    auto method_055() const -> int;
    auto method_092() const -> int;
    auto method_129() const -> int;
    auto method_016() const -> int;
    auto method_053() const -> int;
    auto method_090() const -> int;
    auto method_127() const -> int;
    auto method_014() const -> int;
    auto method_051() const -> int;
    auto method_088() const -> int;
    auto method_125() const -> int;
    auto method_012() const -> int;
    auto method_049() const -> int;
    auto method_086() const -> int;
    auto method_123() const -> int;
    auto method_010() const -> int;
    auto method_047() const -> int;
    auto method_084() const -> int;
    auto method_121() const -> int;
    auto method_008() const -> int;
    auto method_045() const -> int;
    auto method_082() const -> int;
    auto method_119() const -> int;
    auto method_006() const -> int;
    auto method_043() const -> int;
    auto method_080() const -> int;
    auto method_117() const -> int;
    auto method_004() const -> int;
    auto method_041() const -> int;
    auto method_078() const -> int;
    auto method_115() const -> int;
    auto method_002() const -> int;
    auto method_039() const -> int;
    auto method_076() const -> int;
    auto method_113() const -> int;
    auto method_075(int) const -> int;
  };

  constexpr auto groups =
      std::define_static_array(reflex::py::detail::bindable_method_groups(^^wide));

  consteval auto declared_without_parameters() -> std::vector<std::meta::info>
  {
    return std::meta::members_of(^^wide, std::meta::access_context::current())
         | std::views::filter([](std::meta::info m) {
             return std::meta::is_function(m) and not std::meta::is_special_member_function(m)
                and std::meta::parameters_of(m).empty();
           })
         | std::ranges::to<std::vector>();
  }
} // namespace

static_assert(groups.size() == 150);
static_assert(std::ranges::equal(groups, declared_without_parameters()));
static_assert(not std::ranges::is_sorted(groups, {}, std::meta::identifier_of));
