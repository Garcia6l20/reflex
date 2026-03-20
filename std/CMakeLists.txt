add_library(reflex.std OBJECT dummy.cpp)
target_compile_options(reflex.std PUBLIC -freflection -fimplicit-constexpr)
target_compile_features(reflex.std PUBLIC cxx_std_26)
target_compile_definitions(reflex.std PUBLIC $<$<CONFIG:Debug>:DEBUG> $<$<CONFIG:Release>:NDEBUG>)
target_precompile_headers(reflex.std PUBLIC
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
  <iostream>
  <filesystem>
  <chrono>
  <thread>
  <mutex>
  <condition_variable>
  <atomic>
  <coroutine>
  <generator>
  <functional>
)
