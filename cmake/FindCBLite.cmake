include(FindPackageHandleStandardArgs)

set(CPPWIKI_CBLITE_ROOT
    ""
    CACHE PATH "Path to extracted Couchbase Lite for C package"
)

find_package(
  CouchbaseLite CONFIG QUIET
  HINTS "${CPPWIKI_CBLITE_ROOT}"
  PATH_SUFFIXES
    lib/cmake/CouchbaseLite
    lib/x86_64-linux-gnu/cmake/CouchbaseLite
    lib/aarch64-linux-gnu/cmake/CouchbaseLite
)

if(CouchbaseLite_FOUND AND TARGET cblite)
  set(CBLite_FOUND TRUE)
  if(NOT TARGET CBLite::cblite)
    add_library(CBLite::cblite ALIAS cblite)
  endif()
  return()
endif()

find_path(CBLITE_INCLUDE_DIR
  NAMES cbl/CouchbaseLite.h cbl++/CouchbaseLite.hh
  HINTS "${CPPWIKI_CBLITE_ROOT}/include"
)

find_library(CBLITE_LIBRARY
  NAMES cblite libcblite libcblite.so.4 libcblite.so.4.0.3
  HINTS "${CPPWIKI_CBLITE_ROOT}/lib"
  PATH_SUFFIXES x86_64-linux-gnu aarch64-linux-gnu
)

find_package_handle_standard_args(CBLite
  REQUIRED_VARS CBLITE_INCLUDE_DIR CBLITE_LIBRARY
  REASON_FAILURE_MESSAGE
    "Set CPPWIKI_CBLITE_ROOT to an extracted Couchbase Lite for C package containing include/ and lib/ directories."
)

if(CBLite_FOUND AND NOT TARGET CBLite::cblite)
  add_library(CBLite::cblite UNKNOWN IMPORTED)
  set_target_properties(CBLite::cblite PROPERTIES
    IMPORTED_LOCATION "${CBLITE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CBLITE_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(CBLITE_INCLUDE_DIR CBLITE_LIBRARY)
