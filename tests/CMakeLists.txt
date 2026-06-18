add_executable(cppwiki_bridge_tests
  bridge/editor_bridge_test.cc
  ${CMAKE_SOURCE_DIR}/src/bridge/editor_bridge.cc
  ${CMAKE_SOURCE_DIR}/src/bridge/editor_bridge.h
  ${CMAKE_SOURCE_DIR}/src/document/block_note_snapshot.h
  ${CMAKE_SOURCE_DIR}/src/document/document.h
  ${CMAKE_SOURCE_DIR}/src/document/document_validator.cc
  ${CMAKE_SOURCE_DIR}/src/document/document_validator.h
  ${CMAKE_SOURCE_DIR}/src/core/constants.h
  ${CMAKE_SOURCE_DIR}/src/core/qt_string.h
  ${CMAKE_SOURCE_DIR}/src/core/uuid.cc
  ${CMAKE_SOURCE_DIR}/src/core/uuid.h
)

target_include_directories(cppwiki_bridge_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_bridge_tests
  PRIVATE
    Qt6::Core
    spdlog::spdlog
    reflectcpp::reflectcpp
)

cppwiki_apply_common_options(cppwiki_bridge_tests)

add_test(
  NAME cppwiki_bridge_tests
  COMMAND cppwiki_bridge_tests
)

add_executable(cppwiki_document_validator_tests
  document/document_validator_test.cc
  ${CMAKE_SOURCE_DIR}/src/document/block_note_snapshot.h
  ${CMAKE_SOURCE_DIR}/src/document/document.h
  ${CMAKE_SOURCE_DIR}/src/document/document_validator.cc
  ${CMAKE_SOURCE_DIR}/src/document/document_validator.h
)

target_include_directories(cppwiki_document_validator_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_document_validator_tests
  PRIVATE
    Qt6::Core
    spdlog::spdlog
    reflectcpp::reflectcpp
)

cppwiki_apply_common_options(cppwiki_document_validator_tests)

add_test(
  NAME cppwiki_document_validator_tests
  COMMAND cppwiki_document_validator_tests
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
    spdlog::spdlog
)

cppwiki_apply_common_options(cppwiki_program_settings_tests)

add_test(
  NAME cppwiki_program_settings_tests
  COMMAND cppwiki_program_settings_tests
)

add_executable(cppwiki_local_document_repository_tests
  storage/local_document_repository_test.cc
  ${CMAKE_SOURCE_DIR}/src/document/block_note_snapshot.h
  ${CMAKE_SOURCE_DIR}/src/document/document.h
  ${CMAKE_SOURCE_DIR}/src/storage/local_document_repository.h
)

target_include_directories(cppwiki_local_document_repository_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_local_document_repository_tests
  PRIVATE
    spdlog::spdlog
    reflectcpp::reflectcpp
)

cppwiki_apply_common_options(cppwiki_local_document_repository_tests)

add_test(
  NAME cppwiki_local_document_repository_tests
  COMMAND cppwiki_local_document_repository_tests
)

if(CPPWIKI_ENABLE_CBLITE_STORAGE)
  add_executable(cppwiki_cblite_repository_tests
    storage/cblite_repository_test.cc
    ${CMAKE_SOURCE_DIR}/src/core/constants.h
    ${CMAKE_SOURCE_DIR}/src/document/block_note_snapshot.h
    ${CMAKE_SOURCE_DIR}/src/document/document.h
    ${CMAKE_SOURCE_DIR}/src/storage/cblite_document_repository.cc
    ${CMAKE_SOURCE_DIR}/src/storage/cblite_document_repository.h
    ${CMAKE_SOURCE_DIR}/src/storage/local_document_repository.h
  )

  target_include_directories(cppwiki_cblite_repository_tests
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
  )

  target_link_libraries(cppwiki_cblite_repository_tests
    PRIVATE
      CBLite::cblite
      spdlog::spdlog
      reflectcpp::reflectcpp
  )

  cppwiki_apply_common_options(cppwiki_cblite_repository_tests)

  add_test(
    NAME cppwiki_cblite_repository_tests
    COMMAND cppwiki_cblite_repository_tests
  )

  add_executable(cppwiki_cblite_cpp_smoke_tests
    storage/cblite_cpp_smoke_test.cc
  )

  target_link_libraries(cppwiki_cblite_cpp_smoke_tests
    PRIVATE
      CBLite::cblite
      spdlog::spdlog
  )

  cppwiki_apply_common_options(cppwiki_cblite_cpp_smoke_tests)

  add_test(
    NAME cppwiki_cblite_cpp_smoke_tests
    COMMAND cppwiki_cblite_cpp_smoke_tests
  )
endif()

if(CPPWIKI_BUILD_SERVER)
  add_executable(cppwiki_server_config_tests
    server/config/server_config_test.cc
    ${CMAKE_SOURCE_DIR}/src/server/config/server_config.cc
    ${CMAKE_SOURCE_DIR}/src/server/config/server_config.h
    ${CMAKE_SOURCE_DIR}/src/server/config/server_config_dto.h
    ${CMAKE_SOURCE_DIR}/src/core/constants.h
    ${CMAKE_SOURCE_DIR}/src/core/logging.cc
    ${CMAKE_SOURCE_DIR}/src/core/logging.h
  )

  target_include_directories(cppwiki_server_config_tests
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
  )

  target_link_libraries(cppwiki_server_config_tests
    PRIVATE
      spdlog::spdlog
      yaml-cpp::yaml-cpp
      reflectcpp::reflectcpp
  )

  cppwiki_apply_common_options(cppwiki_server_config_tests)

  add_test(
    NAME cppwiki_server_config_tests
    COMMAND cppwiki_server_config_tests
  )

  add_executable(cppwiki_server_health_dto_tests
    server/dto/health_dto_test.cc
    ${CMAKE_SOURCE_DIR}/src/server/dto/api_dto.h
    ${CMAKE_SOURCE_DIR}/src/core/constants.h
  )

  target_include_directories(cppwiki_server_health_dto_tests
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
  )

  target_link_libraries(cppwiki_server_health_dto_tests
    PRIVATE
      spdlog::spdlog
      oatpp::oatpp
  )

  cppwiki_apply_common_options(cppwiki_server_health_dto_tests)

  add_test(
    NAME cppwiki_server_health_dto_tests
    COMMAND cppwiki_server_health_dto_tests
  )
endif()
