cmake_minimum_required(VERSION 3.27.0)
project(tosu_injector)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_EXTENSIONS        ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(
  ${PROJECT_NAME}
  main.cc
)

target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE
  NOMINMAX
  VC_EXTRALEAN
  WIN32_LEAN_AND_MEAN
)
