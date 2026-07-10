# C++ SDK Integration Example

This directory contains a standalone example showing how to consume the XYO C++ SDK from an external application using Conan 2.

## Build and Run with Conan 2

### 1. Build and install the SDK locally
Before running the example, build and export the SDK package into your local Conan cache:
```sh
# From the cpp/ directory:
conan export .
```

### 2. Configure and build the example
Install the dependencies and build the application:
```sh
# From the cpp/example/ directory:
conan install . --build=missing
cmake --preset conan-release
cmake --build build/Release
```

### 3. Run the executable
```sh
./build/Release/xyo_example
```
