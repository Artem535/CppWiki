add_executable(cppwiki_bridge_tests
  bridge/editor_bridge_test.cc
  ${CMAKE_SOURCE_DIR}/src/bridge/editor_bridge.cc
  ${CMAKE_SOURCE_DIR}/src/bridge/editor_bridge.h
  ${CMAKE_SOURCE_DIR}/src/auth/ai_api_key_store.cc
  ${CMAKE_SOURCE_DIR}/src/auth/ai_api_key_store.h
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
    Qt6::Network
    qt6keychain
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

add_executable(cppwiki_file_document_repository_tests
  storage/file_document_repository_test.cc
  ${CMAKE_SOURCE_DIR}/src/document/block_note_snapshot.h
  ${CMAKE_SOURCE_DIR}/src/document/document.h
  ${CMAKE_SOURCE_DIR}/src/storage/file_document_repository.cc
  ${CMAKE_SOURCE_DIR}/src/storage/file_document_repository.h
  ${CMAKE_SOURCE_DIR}/src/storage/local_document_repository.h
)

target_include_directories(cppwiki_file_document_repository_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_file_document_repository_tests
  PRIVATE
    spdlog::spdlog
    reflectcpp::reflectcpp
)

cppwiki_apply_common_options(cppwiki_file_document_repository_tests)

add_test(
  NAME cppwiki_file_document_repository_tests
  COMMAND cppwiki_file_document_repository_tests
)

add_executable(cppwiki_document_sync_service_tests
  sync/document_sync_service_test.cc
  ${CMAKE_SOURCE_DIR}/src/app/program_settings.cc
  ${CMAKE_SOURCE_DIR}/src/app/program_settings.h
  ${CMAKE_SOURCE_DIR}/src/core/constants.h
  ${CMAKE_SOURCE_DIR}/src/core/qt_string.h
  ${CMAKE_SOURCE_DIR}/src/sync/document_sync_service.cc
  ${CMAKE_SOURCE_DIR}/src/sync/document_sync_service.h
  ${CMAKE_SOURCE_DIR}/src/sync/sync_service.cc
  ${CMAKE_SOURCE_DIR}/src/sync/sync_bootstrap.cc
  ${CMAKE_SOURCE_DIR}/src/sync/sync_bootstrap.h
  ${CMAKE_SOURCE_DIR}/src/storage/local_document_repository.h
)

target_include_directories(cppwiki_document_sync_service_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(cppwiki_document_sync_service_tests
  PRIVATE
    CPPWIKI_EDITOR_DIST_DIR="${CMAKE_SOURCE_DIR}/frontend/editor/dist"
)

target_link_libraries(cppwiki_document_sync_service_tests
  PRIVATE
    Qt6::Core
    spdlog::spdlog
)

if(CPPWIKI_ENABLE_CBLITE_STORAGE)
  target_compile_definitions(cppwiki_document_sync_service_tests
    PRIVATE
      CPPWIKI_ENABLE_CBLITE_STORAGE
  )

  target_sources(cppwiki_document_sync_service_tests
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src/storage/cblite_document_repository.cc
      ${CMAKE_SOURCE_DIR}/src/storage/cblite_document_repository.h
  )

  target_link_libraries(cppwiki_document_sync_service_tests
    PRIVATE
      CBLite::cblite
      reflectcpp::reflectcpp
  )
endif()

cppwiki_apply_common_options(cppwiki_document_sync_service_tests)

add_test(
  NAME cppwiki_document_sync_service_tests
  COMMAND cppwiki_document_sync_service_tests
)

add_executable(cppwiki_conflict_merge_model_tests
  gui/conflict_merge_model_test.cc
  ${CMAKE_SOURCE_DIR}/src/core/uuid.cc
  ${CMAKE_SOURCE_DIR}/src/core/uuid.h
  ${CMAKE_SOURCE_DIR}/src/document/block_note_snapshot.h
  ${CMAKE_SOURCE_DIR}/src/document/document.h
  ${CMAKE_SOURCE_DIR}/src/document/document_validator.cc
  ${CMAKE_SOURCE_DIR}/src/document/document_validator.h
  ${CMAKE_SOURCE_DIR}/src/gui/merge/conflict_merge_model.cc
  ${CMAKE_SOURCE_DIR}/src/gui/merge/conflict_merge_model.h
  ${CMAKE_SOURCE_DIR}/src/storage/local_document_repository.h
)

target_include_directories(cppwiki_conflict_merge_model_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_conflict_merge_model_tests
  PRIVATE
    Qt6::Core
    spdlog::spdlog
    reflectcpp::reflectcpp
)

cppwiki_apply_common_options(cppwiki_conflict_merge_model_tests)

add_test(
  NAME cppwiki_conflict_merge_model_tests
  COMMAND cppwiki_conflict_merge_model_tests
)

add_executable(cppwiki_document_tree_model_tests
  gui/document_tree_model_test.cc
  ${CMAKE_SOURCE_DIR}/src/gui/document_tree_model.cc
  ${CMAKE_SOURCE_DIR}/src/gui/document_tree_model.h
  ${CMAKE_SOURCE_DIR}/src/storage/local_document_repository.h
)

target_include_directories(cppwiki_document_tree_model_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_document_tree_model_tests
  PRIVATE
    Qt6::Core
    Qt6::Gui
    reflectcpp::reflectcpp
)

cppwiki_apply_common_options(cppwiki_document_tree_model_tests)

add_test(
  NAME cppwiki_document_tree_model_tests
  COMMAND cppwiki_document_tree_model_tests
)

add_executable(cppwiki_conflict_merge_resolution_tests
  gui/conflict_merge_resolution_test.cc
  ${CMAKE_SOURCE_DIR}/src/document/block_note_snapshot.h
  ${CMAKE_SOURCE_DIR}/src/document/document.h
  ${CMAKE_SOURCE_DIR}/src/document/document_validator.cc
  ${CMAKE_SOURCE_DIR}/src/document/document_validator.h
  ${CMAKE_SOURCE_DIR}/src/gui/merge/conflict_merge_resolution.cc
  ${CMAKE_SOURCE_DIR}/src/gui/merge/conflict_merge_resolution.h
  ${CMAKE_SOURCE_DIR}/src/storage/local_document_repository.h
)

target_include_directories(cppwiki_conflict_merge_resolution_tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_conflict_merge_resolution_tests
  PRIVATE
    Qt6::Core
    spdlog::spdlog
    reflectcpp::reflectcpp
)

cppwiki_apply_common_options(cppwiki_conflict_merge_resolution_tests)

add_test(
  NAME cppwiki_conflict_merge_resolution_tests
  COMMAND cppwiki_conflict_merge_resolution_tests
)

if(CPPWIKI_ENABLE_CBLITE_STORAGE)
  add_executable(cppwiki_cblite_repository_tests
    storage/cblite_repository_test.cc
    ${CMAKE_SOURCE_DIR}/src/core/constants.h
    ${CMAKE_SOURCE_DIR}/src/document/block_note_snapshot.h
    ${CMAKE_SOURCE_DIR}/src/document/document.h
    ${CMAKE_SOURCE_DIR}/src/sync/sync_bootstrap.cc
    ${CMAKE_SOURCE_DIR}/src/sync/sync_bootstrap.h
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
      Qt6::Core
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
    ${CMAKE_SOURCE_DIR}/src/server/config/runtime_config.cc
    ${CMAKE_SOURCE_DIR}/src/server/config/runtime_config.h
    ${CMAKE_SOURCE_DIR}/src/server/config/static_config_factory.cc
    ${CMAKE_SOURCE_DIR}/src/server/config/static_config_factory.h
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
      CLI11::CLI11
      spdlog::spdlog
      reflectcpp::reflectcpp
      yaml-cpp
  )

  cppwiki_apply_common_options(cppwiki_server_config_tests)

  add_test(
    NAME cppwiki_server_config_tests
    COMMAND cppwiki_server_config_tests
  )

  add_executable(cppwiki_server_health_handler_tests
    server/http/health_handler_test.cc
    ${CMAKE_SOURCE_DIR}/src/server/dto/health_response.cc
    ${CMAKE_SOURCE_DIR}/src/server/dto/health_response.h
    ${CMAKE_SOURCE_DIR}/src/server/dto/response_envelope.cc
    ${CMAKE_SOURCE_DIR}/src/server/dto/response_envelope.h
    ${CMAKE_SOURCE_DIR}/src/core/constants.h
    ${CMAKE_SOURCE_DIR}/src/core/logging.cc
    ${CMAKE_SOURCE_DIR}/src/core/logging.h
  )

  target_include_directories(cppwiki_server_health_handler_tests
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
  )

  target_link_libraries(cppwiki_server_health_handler_tests
    PRIVATE
      spdlog::spdlog
      reflectcpp::reflectcpp
      userver::core
  )

  cppwiki_apply_common_options(cppwiki_server_health_handler_tests)

  add_test(
    NAME cppwiki_server_health_handler_tests
    COMMAND cppwiki_server_health_handler_tests
  )

  add_executable(cppwiki_server_lock_stub_tests
    server/http/lock_stub_test.cc
    ${CMAKE_SOURCE_DIR}/src/server/service/lock_service.cc
    ${CMAKE_SOURCE_DIR}/src/server/service/lock_service.h
    ${CMAKE_SOURCE_DIR}/src/core/logging.cc
    ${CMAKE_SOURCE_DIR}/src/core/logging.h
  )

  target_include_directories(cppwiki_server_lock_stub_tests
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
  )

  target_link_libraries(cppwiki_server_lock_stub_tests
    PRIVATE
      spdlog::spdlog
  )

  cppwiki_apply_common_options(cppwiki_server_lock_stub_tests)

  add_test(
    NAME cppwiki_server_lock_stub_tests
    COMMAND cppwiki_server_lock_stub_tests
  )

  add_executable(cppwiki_server_auth_checker_tests
    server/middleware/auth_checker_test.cc
    ${CMAKE_SOURCE_DIR}/src/server/middleware/auth_checker_impl.h
  )

  target_include_directories(cppwiki_server_auth_checker_tests
    PRIVATE
      ${CMAKE_SOURCE_DIR}/src
  )

  target_link_libraries(cppwiki_server_auth_checker_tests
    PRIVATE
      spdlog::spdlog
      userver::core
  )

  cppwiki_apply_common_options(cppwiki_server_auth_checker_tests)

  add_test(
    NAME cppwiki_server_auth_checker_tests
    COMMAND cppwiki_server_auth_checker_tests
  )
endif()
