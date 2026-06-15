# Couchbase Lite C/C++ Integration Spike

**Product:** CppWiki / Wiki Platform v9 - Block Document Edition  
**Status:** Investigation result / delivery wiring added  
**Date:** 2026-06-14  
**Related docs:** `doc/roadmap/Realistic_Delivery_Pipeline.md`, `doc/architecture/Architecture_Baseline_Libraries_and_Approaches.md`

---

# 1. Summary

Couchbase Lite is usable from this C++/Qt application through Couchbase Lite for C (`libcblite`). The practical integration path is not `vcpkg` and not Fedora's `libcouchbase`.

Selected package for the first storage spike:

```text
Couchbase Lite for C Community Edition 4.0.3
```

Recommended approach:

1. Keep a storage abstraction in the app first.
2. Add a `CBLite::cblite` imported CMake target.
3. Let developers point CMake at an extracted Couchbase Lite binary release with `CPPWIKI_CBLITE_ROOT`.
4. Use Couchbase Lite's C++ wrapper inside the storage adapter only.
5. Later add platform-specific packaging for app distribution.

This keeps the app portable and avoids coupling the build to one Linux package manager.

---

# 2. Local Environment Findings

## vcpkg

Local check:

```text
vcpkg search couchbase
vcpkg search couchbase-lite
```

Result: no Couchbase Lite package was listed.

Conclusion: do not add Couchbase Lite to `vcpkg.json` unless we create and maintain an overlay port.

## Fedora / RPM

Local check:

```text
dnf list --available '*cblite*' '*couchbase*'
```

Available packages:

```text
libcouchbase
libcouchbase-devel
libcouchbase-libev
libcouchbase-libevent
libcouchbase-libuv
libcouchbase-tools
```

These are not Couchbase Lite. `libcouchbase` is the Couchbase Server C client SDK, not the embedded offline database used by Couchbase Lite.

Local checks also found no installed `libcblite` through:

```text
pkg-config --list-all
ldconfig -p
find /usr/include /usr/local/include ...
```

Conclusion: the current machine does not already have Couchbase Lite installed.

---

# 3. Official Installation Options

Official Couchbase docs describe Couchbase Lite for C as a binary release containing:

- `include` - headers;
- `lib` - core library binaries;
- `bin` - Windows DLLs.

For Linux, the documented package-manager path is Debian/Ubuntu APT:

```text
sudo apt install libcblite-dev
sudo apt install libcblite-dev-community
```

For local `.deb` installation, the docs say the compiler must link with:

```text
-lcblite
```

For macOS, Couchbase documents Homebrew packages:

```text
brew install libcblite
brew install libcblite-community
```

The current development environment is Fedora/RPM, and the standard Fedora repositories do not expose `libcblite`. Therefore, the portable route is an extracted binary release or a future custom package/overlay.

---

# 4. C++ Wrapper Decision

Couchbase Lite for C exposes a stable plain C API through headers such as:

```cpp
#include "cbl/CouchbaseLite.h"
```

The upstream repository also includes a C++ wrapper API under `include/cbl++`, for example:

```cpp
#include "cbl++/CouchbaseLite.hh"
```

Couchbase marks this wrapper as volatile, so the API may change between releases. We will still use it because it gives a much better implementation surface for C++ code than direct C handles.

Decision: use the C++ wrapper, but only inside `storage/cblite_*` files. The rest of the application must depend on our repository interfaces and DTOs, not on `cbl++` types.

This gives us the ergonomics of C++ while keeping the replacement boundary small if the wrapper changes.

---

# 5. Proposed CMake Integration

Add a local find module:

```text
cmake/FindCBLite.cmake
```

The module first tries the official `CouchbaseLiteConfig.cmake` shipped in the binary package. The official imported target is named `cblite`; the project normalizes it behind `CBLite::cblite`.

If the official config is not found, the module falls back to direct header/library lookup under `CPPWIKI_CBLITE_ROOT`.

Expected developer configuration:

```bash
cmake --preset debug -DCPPWIKI_CBLITE_ROOT=/path/to/libcblite-community
```

The extracted root should contain:

```text
/path/to/libcblite-community/include
/path/to/libcblite-community/lib
```

Minimal CMake shape:

```cmake
set(CPPWIKI_CBLITE_ROOT "" CACHE PATH "Path to extracted Couchbase Lite for C package")

find_path(CBLITE_INCLUDE_DIR
  NAMES cbl/CouchbaseLite.h cbl++/CouchbaseLite.hh
  HINTS "${CPPWIKI_CBLITE_ROOT}/include"
)

find_library(CBLITE_LIBRARY
  NAMES cblite libcblite
  HINTS "${CPPWIKI_CBLITE_ROOT}/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CBLite
  REQUIRED_VARS CBLITE_INCLUDE_DIR CBLITE_LIBRARY
)

if(CBLite_FOUND AND NOT TARGET CBLite::cblite)
  add_library(CBLite::cblite UNKNOWN IMPORTED)
  set_target_properties(CBLite::cblite PROPERTIES
    IMPORTED_LOCATION "${CBLITE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CBLITE_INCLUDE_DIR}"
  )
endif()
```

Then the storage target can link:

```cmake
target_link_libraries(cppwiki_storage
  PRIVATE
    CBLite::cblite
)
```

Do not link Couchbase Lite directly into GUI code. Link it only into a storage adapter target.

---

# 6. Runtime Packaging Notes

The application must be able to find the Couchbase Lite shared library at runtime.

## 6.1. Delivery Decision

We deliver Couchbase Lite as a prebuilt third-party runtime dependency, not as a `vcpkg` dependency and not as a required system package.

The source of truth for the binary should be a pinned Couchbase Lite for C release artifact. We should not commit large binary archives directly into the repository. Instead:

- store the exact version and checksum in repo metadata;
- download or unpack it in CI/dev bootstrap using `scripts/bootstrap-cblite.sh`;
- expose it to CMake through `CPPWIKI_CBLITE_ROOT`;
- copy the runtime library into the application install/package output.

Recommended local layout after bootstrap:

```text
.deps/
  couchbase-lite/
    linux-x86_64/
      include/
      lib/
    macos-arm64/
      include/
      lib/
    windows-x86_64/
      include/
      lib/
      bin/
```

This `.deps` directory is generated and should stay out of git.

## 6.2. Development

For development:

- set `LD_LIBRARY_PATH` to the extracted package `lib` directory; or
- configure an RPATH for local builds.

Preferred developer flow:

```bash
scripts/bootstrap-cblite.sh linux-x86_64

cmake --preset debug \
  -DCPPWIKI_ENABLE_CBLITE_STORAGE=ON \
  -DCPPWIKI_CBLITE_ROOT="$PWD/.deps/couchbase-lite/linux-x86_64"
```

The current pinned metadata points to official Couchbase Lite for C Community Edition 4.0.3 artifacts from `packages.couchbase.com`.

## 6.3. CI

CI should run `scripts/bootstrap-cblite.sh <platform>`. The script fetches the pinned Couchbase Lite archive into the workspace cache, verifies its checksum, unpacks it into `.deps/couchbase-lite/<platform>`, and prints the `CPPWIKI_CBLITE_ROOT` value for CMake.

CI should not install Couchbase Lite globally into the runner image.

## 6.4. Production Package

For production:

- bundle the shared library next to the application binary; or
- place it inside the app bundle/install tree and configure RPATH accordingly.

Expected package layouts:

```text
Linux:
  bin/cppwiki
  lib/libcblite.so

macOS:
  CppWiki.app/
    Contents/MacOS/cppwiki
    Contents/Frameworks/libcblite.dylib

Windows:
  cppwiki.exe
  cblite.dll
```

The CMake install/package step should copy the required Couchbase Lite runtime artifact from `CPPWIKI_CBLITE_ROOT` into the package output.

Linux builds should prefer `$ORIGIN/../lib` RPATH for installed binaries. macOS builds should use `@executable_path/../Frameworks`. Windows builds should place the DLL next to the executable.

Do not assume `/usr/lib` installation. That would make developer and CI machines depend on a system package that may not exist.

## 6.5. Version Pinning Metadata

Add a small metadata file before automating downloads, for example:

```text
third_party/couchbase-lite.version
```

Suggested content:

```text
version=<pinned Couchbase Lite for C version>
linux_x86_64_url=<official release artifact URL>
linux_x86_64_sha256=<checksum>
macos_arm64_url=<official release artifact URL>
macos_arm64_sha256=<checksum>
windows_x86_64_url=<official release artifact URL>
windows_x86_64_sha256=<checksum>
```

The exact version and URLs should be filled when we choose the Couchbase Lite release for the storage spike.

---

# 7. Storage Adapter Shape

Create a repository interface before binding to Couchbase Lite:

```cpp
class LocalDocumentRepository {
 public:
  virtual ~LocalDocumentRepository() = default;

  virtual SaveResult SavePageDocument(const PageDocument& document) = 0;
  virtual LoadResult LoadPageDocument(std::string_view page_id) = 0;
};
```

Then add:

```text
storage/
  local_document_repository.h
  cblite_document_repository.h
  cblite_document_repository.cc
```

The Couchbase Lite adapter should own:

- database open/close;
- collection creation and access (CBL 3.x+ requires explicit collection usage for documents);
- document ID mapping;
- JSON/Fleece conversion;
- save error mapping;
- conflict metadata extraction later.

### Collection API Usage (CBL 3.x+)

In Couchbase Lite C++ 3.x+, documents are accessed through Collections rather than directly through the Database:

```cpp
// Create or get existing collection
auto collection = database_->createCollection("documents");

// Create or get mutable document
auto doc = collection.getMutableDocument(document_id);

// Set properties
doc.setString("title", title_value);
doc.setString("content", content_value);

// Save document
collection.saveDocument(doc);

// Read document
auto doc = collection.getDocument(document_id);
if (doc) {
    auto title = doc.getString("title");
}
```

The default collection (`_default`) is created automatically, but custom collections should be explicitly created for application data separation.

The adapter may include `cbl++/CouchbaseLite.hh`. The rest of the app should not include Couchbase Lite headers or expose Couchbase Lite types in public interfaces.

---

# 8. First Spike Scope

The first Couchbase Lite spike should be intentionally small.

## Scope

- Configure `CPPWIKI_CBLITE_ROOT`.
- Add `FindCBLite.cmake`.
- Add a CTest smoke target behind `CPPWIKI_ENABLE_CBLITE_STORAGE`.
- Use Couchbase Lite's C++ wrapper in the smoke target.
- Add a `storage` target when real persistence starts.
- Open a local database in an app data directory.
- Save one validated page document.
- Load it back after process restart.
- Preserve the previous document on save failure.
- Add a single CTest that writes to a temporary database directory.

## Exit Gate

- CMake fails clearly if `CPPWIKI_CBLITE_ROOT` is missing or invalid.
- Test can create, save, load and delete a temporary database.
- Early smoke test can open, close and delete a temporary database through `cbl::Database`.
- GUI code has no direct Couchbase Lite dependency.
- No public app header exposes `cbl`, `cbl++` or Fleece types.
- Local persistence can be disabled or replaced by a file-backed adapter during development if Couchbase Lite is unavailable.

---

# 9. Risks and Decisions

| Risk / Decision | Assessment | Recommendation |
| :--- | :--- | :--- |
| No `vcpkg` package | Confirmed locally | Do not use `vcpkg.json` for Couchbase Lite now |
| Fedora has `libcouchbase` | Not relevant | Do not use it; it is not embedded Couchbase Lite |
| Official Linux packages are Debian/Ubuntu-oriented | Important for CI/dev setup | Use extracted binary release or custom package for now |
| C++ wrapper is volatile | API risk | Use it only inside `storage/cblite_*`; do not expose it in app-facing APIs |
| Runtime shared library discovery | Packaging risk | Use explicit RPATH/bundling plan |
| Enterprise vs Community | Licensing/product decision | Start with Community for development spikes unless product requirements need Enterprise features |

---

# 10. Recommended Next Action

Do not install Couchbase Lite into the app immediately.

First add document DTOs and validation, then add a `LocalDocumentRepository` interface for the edit-save-restart-load vertical slice. A tiny Couchbase Lite C++ wrapper smoke test exists behind a build option:

```cmake
option(CPPWIKI_ENABLE_CBLITE_STORAGE "Enable Couchbase Lite storage adapter" OFF)
```

Turn the option on only when `CPPWIKI_CBLITE_ROOT` points to a valid extracted Couchbase Lite package.
