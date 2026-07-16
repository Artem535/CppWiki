if (CPPWIKI_BUILD_DESKTOP_APP)
qt_add_executable(cppwiki_app
        MANUAL_FINALIZATION
        WIN32
        MACOSX_BUNDLE
        app/app_context.h
        app/application.cc
        app/application.h
        app/editor_fallback.cc
        app/editor_fallback.h
        app/editor_fallback.html
        app/application_stylesheet.cc
        app/application_stylesheet.h
        app/cppwiki.qss
        app/program_settings.cc
        app/program_settings.h
        auth/auth_token_bundle.h
        auth/auth_session_manager.cc
        auth/auth_session_manager.h
        auth/auth_token_store.cc
        auth/auth_token_store.h
        backend/backend_client.cc
        backend/backend_client.h
        bridge/editor_bridge.cc
        bridge/editor_bridge.h
        core/constants.h
        core/logging.cc
        core/logging.h
        core/qt_string.h
        core/uuid.cc
        core/uuid.h
        gui/merge/conflict_merge_dialog.cc
        gui/merge/conflict_merge_dialog.h
        gui/merge/conflict_merge_model.cc
        gui/merge/conflict_merge_model.h
        gui/merge/conflict_merge_resolution.cc
        gui/merge/conflict_merge_resolution.h
        document/block_note_snapshot.h
        document/document.h
        document/document_validator.cc
        document/document_validator.h
        gui/document_context_menu.cc
        gui/document_context_menu.h
        gui/document_tree_item_delegate.cc
        gui/document_tree_item_delegate.h
        gui/document_tree_model.cc
        gui/document_tree_model.h
        gui/document_tree_view.h
        gui/document_tree_view.cc
        gui/i_page.h
        gui/main_window.cc
        gui/main_window.h
        gui/main_window_helpers.cc
        gui/main_window_helpers.h
        gui/presence_strip_widget.cc
        gui/presence_strip_widget.h
        gui/settings_dialog.cc
        gui/settings_dialog.h
        gui/page.cc
        gui/page.h
        gui/page_helpers.cc
        gui/page_helpers.h

        main.cc
        sync/document_sync_service.cc
        sync/document_sync_service.h
        sync/sync_service.cc
        sync/sync_service.h
        sync/sync_bootstrap.cc
        sync/sync_bootstrap.h
        storage/file_document_repository.cc
        storage/file_document_repository.h
        storage/cblite_document_repository.h
        storage/local_document_repository.h
        storage/repository_factory.cc
        storage/repository_factory.h
)

target_include_directories(cppwiki_app
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(cppwiki_app
        PRIVATE
        CPPWIKI_EDITOR_DIST_DIR="${CMAKE_SOURCE_DIR}/frontend/editor/dist"
)

target_link_libraries(cppwiki_app
        PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::NetworkAuth
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickWidgets
        Qt6::Widgets
        Qt6::WebChannel
        Qt6::WebEngineWidgets
        qt6keychain
        qlementine
        spdlog::spdlog
        reflectcpp::reflectcpp
)

qt_add_resources(cppwiki_app cppwiki_qml_resources
        PREFIX "/cppwiki"
        FILES
        gui/qml/ConflictMergeEditor.qml
)

if (CPPWIKI_ENABLE_CBLITE_STORAGE)
    target_compile_definitions(cppwiki_app
            PRIVATE
            CPPWIKI_ENABLE_CBLITE_STORAGE
    )

    target_sources(cppwiki_app
            PRIVATE
            storage/cblite_document_repository.cc
    )

    target_link_libraries(cppwiki_app
            PRIVATE
            CBLite::cblite
    )
endif ()

cppwiki_apply_common_options(cppwiki_app)

find_program(CPPWIKI_NPM_EXECUTABLE NAMES npm)
if (CPPWIKI_NPM_EXECUTABLE)
    add_custom_target(editor_bundle
            COMMAND ${CPPWIKI_NPM_EXECUTABLE} ci
            COMMAND ${CPPWIKI_NPM_EXECUTABLE} run build
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/frontend/editor
            COMMENT "Building BlockNote editor bundle"
            VERBATIM
    )

    if (CPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP)
        add_dependencies(cppwiki_app editor_bundle)
    endif ()
else ()
    message(WARNING "npm was not found; editor_bundle target will be unavailable")
endif ()

set_target_properties(cppwiki_app
        PROPERTIES
        OUTPUT_NAME cppwiki
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
)

qt_finalize_executable(cppwiki_app)
endif ()

if (CPPWIKI_BUILD_ADMIN_CLI)
    add_executable(cppwiki_admin_cli
            admin/admin_client.cc
            admin/admin_client.h
            admin/admin_http_client.cc
            admin/admin_http_client.h
            admin/admin_response_parsing.cc
            admin/admin_response_parsing.h
            admin/admin_token_store.cc
            admin/admin_token_store.h
            admin/main.cc
    )

    target_include_directories(cppwiki_admin_cli
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(cppwiki_admin_cli
            PRIVATE
            CURL::libcurl
            reflectcpp::reflectcpp
            ftxui::component
            ftxui::dom
            ftxui::screen
    )

    set_target_properties(cppwiki_admin_cli
            PROPERTIES
            OUTPUT_NAME cppwiki-admin
    )

    cppwiki_apply_common_options(cppwiki_admin_cli)
endif ()

if (CPPWIKI_BUILD_SERVER)
    find_package(absl CONFIG REQUIRED)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" CPPWIKI_BUILD_TYPE_UPPER)
    get_target_property(
            CPPWIKI_ABSL_RAW_HASH_SET_ARCHIVE
            absl::raw_hash_set
            "IMPORTED_LOCATION_${CPPWIKI_BUILD_TYPE_UPPER}"
    )
    if (NOT CPPWIKI_ABSL_RAW_HASH_SET_ARCHIVE)
        get_target_property(CPPWIKI_ABSL_RAW_HASH_SET_ARCHIVE absl::raw_hash_set IMPORTED_LOCATION)
    endif ()

    add_executable(cppwiki_server
            server/app/main.cc
            server/app/server_application.cc
            server/app/server_application.h
            server/config/runtime_config.cc
            server/config/runtime_config.h
            server/config/static_config_factory.cc
            server/config/static_config_factory.h
            server/assets/openapi_spec.h
            server/assets/swagger_ui_page.h
            server/components/cppwiki_server_component.cc
            server/components/cppwiki_server_component.h
            server/components/sync_bootstrap_component.cc
            server/components/sync_bootstrap_component.h
            server/handlers/admin_handler.cc
            server/handlers/admin_handler.h
            server/handlers/health_handler.cc
            server/handlers/health_handler.h
            server/handlers/handler_utils.cc
            server/handlers/handler_utils.h
            server/handlers/openapi_handler.cc
            server/handlers/openapi_handler.h
            server/handlers/options_handler.cc
            server/handlers/options_handler.h
            server/handlers/lock_handler.cc
            server/handlers/lock_handler.h
            server/handlers/presence_handler.cc
            server/handlers/presence_handler.h
            server/handlers/sync_config_handler.cc
            server/handlers/sync_config_handler.h
            server/handlers/workspace_handler.cc
            server/handlers/workspace_handler.h
            server/handlers/protected_page_handler.cc
            server/handlers/protected_page_handler.h
            server/handlers/swagger_ui_handler.cc
            server/handlers/swagger_ui_handler.h
            server/middleware/auth_checker_impl.cc
            server/middleware/auth_checker_impl.h
            server/middleware/logging_middleware.cc
            server/middleware/logging_middleware.h
            server/service/lock_service.cc
            server/service/lock_service.h
            server/service/presence_service.cc
            server/service/presence_service.h
            server/service/sync_gateway_adapter.cc
            server/service/sync_gateway_adapter.h
            server/dto/json_adapter.h
            server/dto/admin_response.h
            server/dto/response_envelope.cc
            server/dto/response_envelope.h
            server/dto/health_response.cc
            server/dto/health_response.h
            server/dto/lock_response.cc
            server/dto/lock_response.h
            server/dto/presence_response.cc
            server/dto/presence_response.h
            server/dto/sync_config_response.h
            server/dto/workspace_response.h
            core/constants.h
            core/logging.cc
            core/logging.h
    )

    target_include_directories(cppwiki_server
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(cppwiki_server
            PRIVATE
            CLI11::CLI11
            spdlog::spdlog
            reflectcpp::reflectcpp
            userver::core
            absl::any
            absl::base
            absl::bits
            -Wl,--whole-archive
            ${CPPWIKI_ABSL_RAW_HASH_SET_ARCHIVE}
            -Wl,--no-whole-archive
            re2::re2
            absl::check
            absl::log
            absl::log_initialize
            absl::raw_hash_set
    )

    set_target_properties(cppwiki_server
            PROPERTIES
            OUTPUT_NAME cppwiki-server
    )

    cppwiki_apply_common_options(cppwiki_server)
endif ()
