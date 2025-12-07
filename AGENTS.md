# Repository Guidelines

## Project Structure & Module Organization
Core sources live in `src/`, split between the reusable `appack` library (e.g.,
`compressor.cc`, `database.cc`) and `appack_cli`, the user-facing executable.
`assets/` holds fixtures that the GoogleTest suite hydrates at runtime via
`fixtures_location.h`. Generated build products stay in `build/` (one subdir per
CMake preset), while long-lived helpers sit in `tools/`. External dependencies
are managed through `third_party/` submodules plus `vcpkg.json`; run `just sync`
after cloning to ensure they are present.

## Build, Test, and Development Commands
`just setup preset=debug` seeds a build directory with the chosen CMake preset;
substitute `release` when benchmarking. `just build preset=debug` drives `cmake
--build` and links both the CLI and unit tests. `just test preset=debug` wraps
`ctest` so the right fixtures path is injected automatically. Use `just
list-builds` whenever you need to discover supported presets, and `just clean`
before switching toolchains to avoid ABI drift.

## Coding Style & Naming Conventions
The repo enforces Chromium-style C++20 formatting via `.clang-format`; run
`clang-format -i src/*.cc src/*.h` before posting. Prefer 2-space indentation,
brace-on-same-line for namespaces, and UpperCamelCase for structs/classes (e.g.,
`UniqueObject`). Function names are lower_snake_case; constants use
`kPrefixStyle`. Keep headers self-contained, rely on `absl` utilities already
linked, and favor `absl::StatusOr` patterns when extending error surfaces.

## Testing Guidelines
All tests are written with GoogleTest and automatically registered through
`gtest_discover_tests` in `src/CMakeLists.txt`. Mirror the production namespace
under `src/` when adding new suites (e.g., `mapping_test.cc`). Name tests
`ComponentScenarioExpectation` so failures are scannable, and load needed sample
blobs from `assets/` instead of inventing inline strings. Run `ctest --preset
release --output-on-failure` before merging; new functionality should ship with
coverage of both the library class and the CLI entry point that exercises it.

## Commit & Pull Request Guidelines
Commits follow short, imperative subjects (`Add recommended warnings`, `Wire up
adding files...`). Keep bodies optional; when required, wrap at 72 columns and
explain the "why". PRs must describe motivation, list build/test evidence
(command + preset), and link the tracking issue or task ID. Include screenshots
for CLI UX changes and call out any ABI impacts so downstream packagers can
adjust.
