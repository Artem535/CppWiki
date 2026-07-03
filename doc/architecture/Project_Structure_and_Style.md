# Project Structure and C++ Style Baseline

**Status:** Draft / Implementation baseline  
**Date:** 2026-06-11  
**Applies to:** CppWiki Qt/C++ codebase

---

# 1. Purpose

This document fixes the initial repository structure, CMake layout and C++ style rules for the Qt implementation.

The baseline follows:

- Google C++ Style Guide for C++ naming, self-contained headers, include guards and include ordering;
- Qt 6 CMake target APIs for executable creation and finalization;
- target-based CMake;
- clang-format with `BasedOnStyle: Google`;
- clang-tidy with Google, modernize, bugprone, performance and readability checks.

---

# 2. Repository Layout

```text
.
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
│   └── ProjectOptions.cmake
├── src/
│   ├── CMakeLists.txt
│   ├── main.cc
│   ├── app/
│   │   ├── application.h
│   │   └── application.cc
│   └── gui/
│       ├── i_page.h
│       ├── main_window.h
│       ├── main_window.cc
│       ├── page.h
│       └── page.cc
├── frontend/
│   └── editor/
│       ├── package.json
│       ├── vite.config.ts
│       └── src/
├── doc/
│   └── architecture/
├── third_party/
└── ML LLM Delivery Pipeline/
```

Future modules should be added as narrow CMake targets under `src/`:

```text
src/
  core/
  gui/
  editor_bridge/
  storage/
  auth/
  plugins/
  render/
  confluence/
  server/
    app/
    components/
    config/
    dto/
    handlers/
    middleware/
    service/
```

Use `include/` only when a target intentionally exposes public headers to other targets or external consumers. Internal headers stay beside implementation files in `src/<module>/`.

---

# 3. File Naming

Use Google-style C++ extensions:

- headers: `.h`;
- source files: `.cc`;
- tests: `*_test.cc`.

Every non-test `.cc` should normally have a matching `.h`, except small files whose only purpose is `main()`.

---

# 4. Header Rules

Headers must be self-contained:

- include what they use directly;
- do not rely on transitive includes;
- use include guards, not `#pragma once`;
- keep public declarations readable;
- move non-trivial function bodies to `.cc`.

Include guard format:

```cpp
#ifndef CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
#define CPPWIKI_SRC_GUI_MAIN_WINDOW_H_

// declarations

#endif  // CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
```

Include order:

1. related header;
2. C system headers;
3. C++ standard library headers;
4. third-party and Qt headers;
5. project headers.

Separate non-empty groups with one blank line.

---

# 5. CMake Rules

Use target-based CMake only:

- prefer `target_sources`;
- prefer `target_link_libraries`;
- prefer `target_include_directories`;
- avoid global include directories and global compile flags;
- keep reusable compiler options in `cmake/ProjectOptions.cmake`;
- configure through `CMakePresets.json`.

Qt applications must be created with `qt_add_executable` and finalized with `qt_finalize_executable`.

---

# 6. Qt Application Baseline

The current bootstrap is intentionally small:

- `cppwiki::Application` owns process-level Qt application setup and top-level object wiring;
- `cppwiki::MainWindow` owns the desktop shell window and active page placement;
- `cppwiki::IPage` defines the UI page contract;
- `cppwiki::Page` is the first page implementation and owns the `QWebEngineView` editor host;
- future BlockNote/Tiptap assets will be loaded through an `editor_bridge` module.

The JavaScript editor must not receive raw filesystem, token, database or unrestricted network access.

---

# 7. Formatting and Static Analysis

Formatting:

```bash
clang-format -i src/**/*.h src/**/*.cc
```

Static analysis is configured through `.clang-tidy` but disabled by default in CMake. Enable it with:

```bash
cmake --preset debug -DCPPWIKI_ENABLE_CLANG_TIDY=ON
```

Build commands:

```bash
cmake --preset debug
cmake --build --preset debug
```

The project uses vcpkg manifest mode. Set `VCPKG_ROOT` before configuring:

```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset debug
```

Qt is not installed through vcpkg. Use system Qt packages or an external Qt installation and expose it through `CMAKE_PREFIX_PATH` when CMake cannot find it automatically:

```bash
cmake --preset debug -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
```

Current manifest dependencies:

- `reflectcpp`;
- `spdlog`;
- `cli11`.

The server additionally depends on `userver`, fetched via `FetchContent`.

Wasmtime is part of the architecture baseline, but it is not listed in `vcpkg.json` until a supported vcpkg port or overlay port is selected.

## 7. Frontend Editor Bundle

The BlockNote editor is a local Vite/React bundle under `frontend/editor`.

Development commands:

```bash
cd frontend/editor
npm ci
npm run build
```

The Qt application loads `frontend/editor/dist/index.html` through `QWebEngineView`. If the bundle is missing, the desktop app shows a fallback page with build instructions.

CMake also exposes the explicit frontend target:

```bash
cmake --build --preset debug --target editor_bundle
```

By default, `cppwiki_app` does not depend on `editor_bundle`, so normal C++ builds do not run npm. For linked development builds, configure with:

```bash
cmake --preset debug -DCPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP=ON
```
