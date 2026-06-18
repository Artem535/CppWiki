cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
  set(CMAKE_TOOLCHAIN_FILE
      "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file"
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
option(CPPWIKI_BUILD_SERVER "Build CppWiki oat++ server" ON)
option(CPPWIKI_ENABLE_CLANG_TIDY "Enable clang-tidy when available" OFF)
option(CPPWIKI_ENABLE_CBLITE_STORAGE "Enable Couchbase Lite storage adapter" OFF)
option(
  CPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP
  "Build the frontend editor bundle before building the desktop app"
  OFF
)

include(cmake/ProjectOptions.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(
  Qt6 6.5 REQUIRED
  COMPONENTS Core Gui Widgets WebChannel WebEngineWidgets Svg
)
find_package(reflectcpp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

if(CPPWIKI_BUILD_SERVER)
  find_package(CLI11 CONFIG REQUIRED)
  find_package(oatpp CONFIG REQUIRED)
  find_package(oatpp-swagger CONFIG REQUIRED)
  find_package(oatpp-websocket CONFIG REQUIRED)
  find_package(yaml-cpp CONFIG REQUIRED)
endif()

if(CPPWIKI_ENABLE_CBLITE_STORAGE)
  find_package(CBLite REQUIRED)
endif()

qt_standard_project_setup(REQUIRES 6.5)

set(QLEMENTINE_SANDBOX OFF CACHE BOOL "Build qlementine sandbox" FORCE)
set(QLEMENTINE_SHOWCASE OFF CACHE BOOL "Build qlementine showcase" FORCE)
add_subdirectory(third_party/qlementine)

if(CPPWIKI_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(src)

if(CPPWIKI_BUILD_TESTS)
  add_subdirectory(tests)
endif()
