add_library(std INTERFACE)
target_precompile_headers(std INTERFACE
  <concepts>
  <ranges>
  <string>
  <string_view>
  <type_traits>
  <meta>
  <vector>
  <inplace_vector>
  <array>
  <optional>
  <variant>
  <tuple>
  <utility>
  <initializer_list>
  <memory>
  <span>
  <format>
  <print>
  <source_location>
)
