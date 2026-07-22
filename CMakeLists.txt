cmake_minimum_required(VERSION 3.24)

if (NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE
            "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file"
    )
endif ()

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
option(CPPWIKI_BUILD_SERVER "Build CppWiki userver server" ON)
option(CPPWIKI_BUILD_DESKTOP_APP "Build CppWiki Qt desktop application" ON)
option(CPPWIKI_BUILD_ADMIN_CLI "Build the cppwiki-admin ftxui CLI" ON)
option(CPPWIKI_ENABLE_CLANG_TIDY "Enable clang-tidy when available" OFF)
option(CPPWIKI_ENABLE_CBLITE_STORAGE "Enable Couchbase Lite storage adapter" OFF)
option(
        CPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP
        "Build the frontend editor bundle before building the desktop app"
        OFF
)

include(cmake/ProjectOptions.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchContent)

find_package(reflectcpp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

if (CPPWIKI_BUILD_ADMIN_CLI)
    # cpr wraps libcurl with a modern, requests-like C++ API (cpr::Get/Post,
    # cpr::Header, cpr::Timeout) and no manual curl_easy_*/curl_slist_*
    # calls. CPR_USE_SYSTEM_CURL keeps it linking against the system libcurl
    # (matching this repo's existing `find_package(CURL REQUIRED)` usage)
    # rather than vendoring its own copy.
    set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "Use system libcurl for cpr" FORCE)
    set(CPR_BUILD_TESTS OFF CACHE BOOL "Disable cpr tests" FORCE)
    FetchContent_Declare(
            cpr
            GIT_REPOSITORY https://github.com/libcpr/cpr.git
            GIT_TAG 1.11.2
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cpr)

    set(FTXUI_BUILD_DOCS OFF CACHE BOOL "Disable ftxui docs" FORCE)
    set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "Disable ftxui examples" FORCE)
    set(FTXUI_BUILD_TESTS OFF CACHE BOOL "Disable ftxui tests" FORCE)
    set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "Disable ftxui install rules" FORCE)
    FetchContent_Declare(
            ftxui
            GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui.git
            GIT_TAG v5.0.0
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(ftxui)

    # hrantzsch/keychain wraps the native OS secret store (libsecret on Linux,
    # Keychain Services on macOS, Credential Manager on Windows) without
    # requiring any Qt linkage, unlike qtkeychain below — a better fit for
    # keeping the admin CLI Qt-GUI-free. On Linux it depends on GLib 2.0 and
    # libsecret-1 (already required by qtkeychain's libsecret backend).
    set(BUILD_TESTS OFF CACHE BOOL "Disable hrantzsch/keychain tests" FORCE)
    FetchContent_Declare(
            keychain
            GIT_REPOSITORY https://github.com/hrantzsch/keychain.git
            GIT_TAG v1.3.1
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(keychain)
endif ()

if (CPPWIKI_BUILD_DESKTOP_APP OR CPPWIKI_BUILD_TESTS)
    find_package(
            Qt6 6.5 REQUIRED
            COMPONENTS Core DBus Gui NetworkAuth Widgets WebChannel WebEngineWidgets Svg Qml Quick QuickWidgets
    )
endif ()

if (CPPWIKI_BUILD_DESKTOP_APP)
    set(BUILD_WITH_QT6 ON CACHE BOOL "Build qtkeychain with Qt 6" FORCE)
    set(BUILD_TEST_APPLICATION OFF CACHE BOOL "Disable qtkeychain test app" FORCE)
    set(BUILD_TRANSLATIONS OFF CACHE BOOL "Disable qtkeychain translations" FORCE)
    set(LIBSECRET_SUPPORT ON CACHE BOOL "Enable qtkeychain libsecret backend" FORCE)
    FetchContent_Declare(
            qtkeychain
            GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
            GIT_TAG 0.16.0
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(qtkeychain)
endif ()

if (CPPWIKI_BUILD_DESKTOP_APP OR CPPWIKI_BUILD_TESTS)
    # KDGantt (issue #113/#119): a native Qt model/view Gantt widget, meant to replace the
    # web-based (SVAR react-gantt) Gantt tab in the Project board document kind. KDGantt hasn't
    # shipped as its own repo since ~2016 (github.com/KDE/kdgantt2 is stale and Qt5-only); the
    # actively maintained, Qt6-capable source lives inside KDAB's KDChart repo under src/KDGantt,
    # built into the same `kdchart` library target as the charting code. Verified by reading the
    # actual SPDX headers in the fetched source (e.g. src/KDGantt/kdganttglobal.h) rather than
    # trusting the historical "KDGantt is LGPL" assumption: as of KDChart 3.0.0 the whole repo,
    # KDGantt included, is MIT-licensed (see the fetched LICENSES/MIT.txt).
    set(KDChart_QT6 ON CACHE BOOL "Build KDChart/KDGantt against Qt6" FORCE)
    set(KDChart_STATIC OFF CACHE BOOL "Build KDChart/KDGantt as a shared library" FORCE)
    set(KDChart_TESTS OFF CACHE BOOL "Disable KDChart/KDGantt's own test suite" FORCE)
    set(KDChart_EXAMPLES OFF CACHE BOOL "Disable KDChart/KDGantt's own examples" FORCE)
    set(KDChart_DOCS OFF CACHE BOOL "Disable KDChart/KDGantt API docs" FORCE)
    FetchContent_Declare(
            kdchart
            GIT_REPOSITORY https://github.com/KDAB/KDChart.git
            GIT_TAG kdchart-3.0.0
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(kdchart)
endif ()

if (CPPWIKI_BUILD_SERVER)
    find_package(CLI11 CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    set(USERVER_FEATURE_CORE ON CACHE BOOL "" FORCE)
    set(USERVER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(USERVER_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_GRPC ON CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_OTLP ON CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_CHAOTIC ON CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_STACKTRACE OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
            userver
            GIT_REPOSITORY https://github.com/userver-framework/userver.git
            GIT_TAG v3.0
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(userver)
endif ()

if (CPPWIKI_BUILD_DESKTOP_APP OR CPPWIKI_BUILD_TESTS)
    qt_standard_project_setup(REQUIRES 6.5)
endif ()

if (CPPWIKI_ENABLE_CBLITE_STORAGE)
    find_package(CBLite REQUIRED)
endif ()

if (CPPWIKI_BUILD_DESKTOP_APP)
    set(QLEMENTINE_SANDBOX OFF CACHE BOOL "Build qlementine sandbox" FORCE)
    set(QLEMENTINE_SHOWCASE OFF CACHE BOOL "Build qlementine showcase" FORCE)
    add_subdirectory(third_party/qlementine)
endif ()

if (CPPWIKI_BUILD_TESTS)
    enable_testing()
endif ()

add_subdirectory(src)

if (CPPWIKI_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
