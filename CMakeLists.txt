cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE
      "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file"
  )
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
   AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
   AND NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET
      "x64-linux-dynamic"
      CACHE STRING "Vcpkg target triplet"
  )
endif()

project(
  CppWiki
  VERSION 0.1.0
  DESCRIPTION "Desktop-first offline-first wiki platform"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPPWIKI_BUILD_TESTS "Build CppWiki tests" ON)
option(CPPWIKI_ENABLE_CLANG_TIDY "Enable clang-tidy when available" OFF)

include(cmake/ProjectOptions.cmake)

find_package(
  Qt6 6.5 REQUIRED
  COMPONENTS Core Gui Widgets WebChannel WebEngineWidgets
)
find_package(Drogon CONFIG REQUIRED)
find_package(reflectcpp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(src)

if(CPPWIKI_BUILD_TESTS)
  enable_testing()
endif()
