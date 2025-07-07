# `reflex` project

## Overview

`reflex` leverages cutting-edge C++26 reflection features to deliver high-level utilities for modern C++ development. The project currently provides modules for command-line interfaces (CLI) and dependency injection, with additional features planned for future releases. By utilizing compile-time reflection, `reflex` aims to simplify and automate common programming tasks, making C++ development more expressive and efficient.

**Disclaimer:**  
This project utilizes early C++26 features that are not yet available in mainstream compilers. The implementation currently relies on the [Bloomberg's clang fork](https://github.com/bloomberg/clang-p2996), which provides experimental support for these proposals. Please note that syntax, semantics, and compiler support may change as the standard evolves. Use at your own risk and for experimental purposes only.

## Requirements

- Bloomberg's clang fork: [https://github.com/bloomberg/clang-p2996](https://github.com/bloomberg/clang-p2996)
- CMake (version X.Y or higher)
- Standard C++ build tools

## Building


## Modules

- **reflex.cli**: [Command line interface](lib/cli/README.md).
- **reflex.di**: [Dependency injection](lib/di/README.md).

## Building clang

In llvm project
```bash
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=~/.local -DLLVM_ENABLE_RUNTIMES="clang;libcxx;libcxxabi;libunwind" -DCMAKE_BUILD_TYPE=Release ../llvm
ninja # or make
ninja install # or make install
```
