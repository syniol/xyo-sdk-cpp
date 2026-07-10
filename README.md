# XYO.Financial SDK for C++
The official C++17 SDK for the XYO.Financial transaction enrichment API. A valid API key is available from the [XYO dashboard](https://xyo.financial/dashboard).

## Requirements
- A C++17 compiler
- CMake 3.16 or newer
- libcurl

## Build and test
```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage
```cpp
#include <xyo/client.hpp>

#include <iostream>

int main() {
  xyo::Client client({"YourAPIKeyFromXYO.FinancialDashboard"});

  auto result = client.enrich_transaction({"COSTA PICKUP", "GB"});
  std::cout << result.merchant << '\n';

  auto bulk = client.enrich_transaction_collection({
      {"COSTA PICKUP", "GB"},
      {"STRBUKS GREENWICH", "GB"},
  });
  auto status = client.enrich_transaction_collection_status(bulk.id);
  std::cout << xyo::to_string(status) << '\n';
}
```

Requests that fail at the transport layer, return a non-200 status, or contain an invalid response throw `xyo::Error`. `location` and `address` are represented as `std::optional<std::string>` because the service may return JSON `null`.


## Installation & Integration

The C++ SDK can be integrated into your project using one of the following methods.

### 1. Conan 2 (Recommended)
Add the SDK to your dependencies in your `conanfile.txt` or `conanfile.py`:
```text
[requires]
xyo-sdk-cpp/1.0.0
```
Run `conan install` to retrieve the package and dependencies:
```sh
conan install . --build=missing
```

### 2. vcpkg
Install the package via vcpkg:
```sh
vcpkg install xyo-sdk-cpp
```

### 3. CMake FetchContent (Fallback)
You can include the SDK directly in your `CMakeLists.txt` without pre-installing package managers:
```cmake
include(FetchContent)
FetchContent_Declare(
  xyo-sdk-cpp
  GIT_REPOSITORY https://github.com/syniol/xyo-sdk-cpp.git
  GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(xyo-sdk-cpp)
```

### Consuming in CMake
In your project's `CMakeLists.txt`, link against the target:
```cmake
find_package(XYOSDK CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE XYO::SDK)
```

## License

Copyright © 2025 Syniol Limited. All rights reserved. See [LICENSE](LICENSE).
