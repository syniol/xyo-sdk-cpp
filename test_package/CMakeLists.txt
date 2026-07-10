cmake_minimum_required(VERSION 3.16)
project(test_package LANGUAGES CXX)

find_package(XYOSDK CONFIG REQUIRED)

add_executable(example src/example.cpp)
target_link_libraries(example PRIVATE XYO::SDK)
