cmake_minimum_required(VERSION 3.30)
project(donate)

add_executable(donate src/donate.cpp)
set_property(TARGET donate PROPERTY CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -O3")
