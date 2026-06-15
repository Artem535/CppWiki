add_executable(cppwiki_bridge_tests
  bridge/editor_bridge_test.cc
  ${CMAKE_SOURCE_DIR}/src/bridge/editor_bridge.cc
  ${CMAKE_SOURCE_DIR}/src/bridge/editor_bridge.h
  ${CMAKE_SOURCE_DIR}/src/core/constants.h
  ${CMAKE_SOURCE_DIR}/src/core/qt_string.h
)

target_include_directories(cppwiki_bridge_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_bridge_tests
  PRIVATE
    Qt6::Core
    spdlog::spdlog
)

cppwiki_apply_common_options(cppwiki_bridge_tests)

add_test(
  NAME cppwiki_bridge_tests
  COMMAND cppwiki_bridge_tests
)

add_executable(cppwiki_program_settings_tests
  app/program_settings_test.cc
  ${CMAKE_SOURCE_DIR}/src/app/program_settings.cc
  ${CMAKE_SOURCE_DIR}/src/app/program_settings.h
  ${CMAKE_SOURCE_DIR}/src/core/constants.h
  ${CMAKE_SOURCE_DIR}/src/core/qt_string.h
)

target_include_directories(cppwiki_program_settings_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(cppwiki_program_settings_tests
  PRIVATE
    CPPWIKI_EDITOR_DIST_DIR="${CMAKE_SOURCE_DIR}/frontend/editor/dist"
)

target_link_libraries(cppwiki_program_settings_tests
  PRIVATE
    Qt6::Core
)

cppwiki_apply_common_options(cppwiki_program_settings_tests)

add_test(
  NAME cppwiki_program_settings_tests
  COMMAND cppwiki_program_settings_tests
)

if(CPPWIKI_ENABLE_CBLITE_STORAGE)
  add_executable(cppwiki_cblite_cpp_smoke_tests
    storage/cblite_cpp_smoke_test.cc
  )

  target_link_libraries(cppwiki_cblite_cpp_smoke_tests
    PRIVATE
      CBLite::cblite
  )

  cppwiki_apply_common_options(cppwiki_cblite_cpp_smoke_tests)

  add_test(
    NAME cppwiki_cblite_cpp_smoke_tests
    COMMAND cppwiki_cblite_cpp_smoke_tests
  )
endif()
