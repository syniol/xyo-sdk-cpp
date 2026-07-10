# Contributing to XYO C++ SDK

Thank you for contributing to the XYO C++ SDK! Please follow these guidelines to ensure a smooth development and release process.

## Development Workflow

### Requirements

- A C++17 compliant compiler
- CMake 3.16+
- libcurl developer package (e.g. `libcurl4-openssl-dev` on Ubuntu/Debian)
- Conan 2.x (optional, for packaging tests)

### Build and Run Tests

1. Configure the build:
   ```sh
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DXYO_BUILD_TESTS=ON
   ```
2. Build the project:
   ```sh
   cmake --build build
   ```
3. Run the unit test suite:
   ```sh
   ctest --test-dir build --output-on-failure
   ```

### Code Style

Please format your changes using `clang-format` matching standard C++ core guidelines.

## Release Process

We follow a strict, automated versioning and release workflow:

1. **Verify Blockers:** Ensure all release blockers in the repository issues/checklist are resolved (e.g. timeout, Windows exports, etc.).
2. **Version Bump:** Update the version in `CMakeLists.txt` via:
   ```cmake
   project(xyo_sdk_cpp VERSION X.Y.Z LANGUAGES CXX)
   ```
3. **Changelog:** Update `CHANGELOG.md` with the release notes under the appropriate version section.
4. **Push to main:** Create a pull request to `main` branch. Once CI builds succeed on GCC, Clang, macOS, and Windows, merge the PR.
5. **Git Tag:** Create and push a semver tag matching the CMake project version:
   ```sh
   git tag vX.Y.Z
   git push origin vX.Y.Z
   ```
   The `.github/workflows/release.yml` workflow will trigger automatically, run validation checks, compile release archives, generate SBOMs, sign/attest the build provenance, and upload assets to the GitHub Release.
