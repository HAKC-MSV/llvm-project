# HAKC LLVM Changes

## Scope

The primary implementation is under:

- `llvm/lib/Transforms/Compartmentalization/`
- `llvm/include/llvm/Transforms/Compartmentalization/`
- `llvm/utils/hakc`

Do not inspect or modify unrelated LLVM subsystems unless required to
understand an API or resolve a build failure.

## Upstream Code

Treat the rest of llvm-project as an upstream dependency. Prefer targeted
symbol searches and direct file reads instead of broad repository scans.

## Build

Build the pass with:

    cmake --build cmake-build-hakc --target clang

Run tests with:

    cmake --build cmake-build-hakc --target check-llvm
