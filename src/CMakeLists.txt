if (CPPWIKI_BUILD_DESKTOP_APP)
qt_add_executable(cppwiki_app
        MANUAL_FINALIZATION
        WIN32
        MACOSX_BUNDLE
        app/accent_color.cc
        app/accent_color.h
        app/app_context.h
        app/application.cc
        app/application.h
        app/editor_fallback.cc
        app/editor_fallback.h
        app/editor_fallback.html
        app/application_stylesheet.cc
        app/application_stylesheet.h
        app/program_settings.cc
        app/program_settings.h
        auth/ai_api_key_store.cc
        auth/ai_api_key_store.h
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
        gui/coming_soon_widget.cc
        gui/coming_soon_widget.h
        gui/document_context_menu.cc
        gui/document_context_menu.h
        gui/document_tree_item_delegate.cc
        gui/document_tree_item_delegate.h
        gui/document_tree_model.cc
        gui/document_tree_model.h
        gui/document_tree_view.h
        gui/document_tree_view.cc
        gui/import_destination_dialog.cc
        gui/import_destination_dialog.h
        gui/i_page.h
        gui/main_window.cc
        gui/main_window.h
        gui/main_window_helpers.cc
        gui/main_window_helpers.h
        gui/presence_strip_widget.cc
        gui/presence_strip_widget.h
        gui/settings_dialog.cc
        gui/settings_dialog.h
        gui/trash_dialog.cc
        gui/trash_dialog.h
        gui/page.cc
        gui/page.h
        gui/attachment_url_scheme_handler.cc
        gui/attachment_url_scheme_handler.h
        gui/page_helpers.cc
        gui/page_helpers.h
        gui/project_board/project_board_native_widget.cc
        gui/project_board/project_board_native_widget.h
        gui/revision_history_dialog.cc
        gui/revision_history_dialog.h
        gui/workspace_rail_widget.cc
        gui/workspace_rail_widget.h

        main.cc
        sync/document_sync_service.cc
        sync/document_sync_service.h
        sync/sync_service.cc
        sync/sync_service.h
        sync/sync_bootstrap.cc
        sync/sync_bootstrap.h
        storage/file_document_repository.cc
        storage/file_document_repository.h
        storage/attachment.cc
        storage/attachment.h
        storage/cblite_document_repository.h
        storage/local_document_repository.h
        storage/repository_factory.cc
        storage/repository_factory.h
        storage/workspace_archive.cc
        storage/workspace_archive.h
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

qt_add_resources(cppwiki_app cppwiki_icon_resources
        PREFIX "/cppwiki/icons"
        BASE gui/assets/icons
        FILES
        gui/assets/icons/rail-documents.svg
        gui/assets/icons/rail-ai-chat.svg
        gui/assets/icons/rail-code.svg
        gui/assets/icons/document-wiki-page.svg
        gui/assets/icons/document-jupyter-notebook.svg
        gui/assets/icons/document-excalidraw-canvas.svg
        gui/assets/icons/document-openapi-spec.svg
        gui/assets/icons/document-project-board.svg
        # Bundled monochrome action icons (issue #182), replacing QIcon::fromTheme()/
        # style()->standardIcon() call sites: the freedesktop icon theme QIcon::fromTheme()
        # resolves against doesn't exist at all on Windows (every such call returns a blank
        # icon there) and isn't reliably present in a bare AppImage sandbox on Linux either.
        # Sourced from Lucide (https://lucide.dev, ISC license), recolored to match the
        # existing rail-*/document-*.svg icons' fixed stroke color.
        gui/assets/icons/action-settings.svg
        gui/assets/icons/action-trash.svg
        gui/assets/icons/action-folder.svg
        gui/assets/icons/action-folder-open.svg
        gui/assets/icons/action-file-plus.svg
        gui/assets/icons/action-file.svg
        gui/assets/icons/action-pencil.svg
        gui/assets/icons/action-history.svg
        gui/assets/icons/action-refresh.svg
        gui/assets/icons/action-move-up.svg
        gui/assets/icons/action-move-down.svg
        gui/assets/icons/action-new-folder.svg
)

# Embedded as a Qt resource (not a filesystem path guessed relative to CWD/applicationDirPath())
# because kQlementineDarkThemePath used to point at
# third_party/qlementine/showcase/resources/themes/dark.json -- a vendored submodule *demo*
# resource, resolvable only by accident when the working directory or build-output depth happened
# to line up with ResolveThemePath()'s relative-path guesses. It was never installed/packaged, so
# every packaged build (AppImage/DMG/installer) silently fell back to QlementineStyle's default
# (light) theme with no error. QFile (which Theme::fromJsonPath() uses) supports ":/" resource
# paths natively, so embedding it here fixes every context at once.
qt_add_resources(cppwiki_app cppwiki_theme_resources
        PREFIX "/cppwiki/themes"
        BASE app/themes
        FILES
        app/themes/dark.json
)

# Embedded as a Qt resource for the same reason as cppwiki_theme_resources above (issue #182):
# kApplicationQssPath used to be a raw "src/app/cppwiki.qss" filesystem path that
# ResolveApplicationStylesheetPath() guessed at relative to CWD/applicationDirPath(). It was
# never installed/packaged anywhere, so every packaged build (AppImage/DMG/installer) silently
# fell back to only the small ADR-016 accent-tint stylesheet, dropping every pill/rounded-corner/
# elevated-card rule cppwiki.qss actually defines -- while the dark background color kept showing
# through regardless, since that comes from the separately-embedded theme JSON above via
# QApplication::setPalette(), independent of QSS. Embedding this file here fixes every context
# at once, the same way it did for dark.json.
qt_add_resources(cppwiki_app cppwiki_qss_resources
        PREFIX "/cppwiki/app"
        BASE app
        FILES
        app/cppwiki.qss
)

# Bundled UI font (issue #182): previously no application font family was ever set, so text
# rendering depended entirely on whatever fonts happened to be installed on the target machine
# -- unreliable especially in a bare AppImage sandbox or a fresh Windows install. Inter (OFL
# 1.1, see app/fonts/OFL.txt) as a single variable-weight TTF covers Thin through Black in one
# file, loaded via QFontDatabase::addApplicationFont() in Application's constructor and set as
# the app's default font family.
qt_add_resources(cppwiki_app cppwiki_font_resources
        PREFIX "/cppwiki/fonts"
        BASE app/fonts
        FILES
        app/fonts/Inter-Variable.ttf
)

# Application icon (issue #182): Qt itself embeds no icon resource, so without these, packaged
# builds show a generic default executable/bundle icon in the taskbar/Dock/Explorer instead of
# CppWiki's own logo (doc/pic/icon.png). Windows: cppwiki.rc embeds packaging/windows/cppwiki.ico
# (a multi-resolution .ico generated from doc/pic/icon.png) as a resource directly in
# cppwiki.exe -- CppWiki.iss's SetupIconFile covers the installer/uninstaller wizard separately.
# macOS: MACOSX_BUNDLE_ICON_FILE + MACOSX_PACKAGE_LOCATION "Resources" wires
# packaging/macos/cppwiki.icns into cppwiki.app's Info.plist/Resources, the CMake-native way to
# set a bundle icon (see CMake's MACOSX_BUNDLE_ICON_FILE docs). Linux has no equivalent needed
# here: the AppImage's icon is staged directly from doc/pic/icon.png by
# .github/workflows/package-release.yml's "Stage desktop file and icon" step.
if (WIN32)
    target_sources(cppwiki_app PRIVATE ${CMAKE_SOURCE_DIR}/packaging/windows/cppwiki.rc)
elseif (APPLE)
    target_sources(cppwiki_app PRIVATE ${CMAKE_SOURCE_DIR}/packaging/macos/cppwiki.icns)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/packaging/macos/cppwiki.icns PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_target_properties(cppwiki_app PROPERTIES MACOSX_BUNDLE_ICON_FILE cppwiki.icns)
endif ()

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

install(TARGETS cppwiki_app
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
)

# kdchart and qt6keychain build as real shared libraries (KDChart_STATIC OFF above; qtkeychain
# defaults to shared) but ship with no install() rules of their own -- without this, a packaged
# app is silently missing a required runtime library. Co-locate them next to the executable: on
# Windows/Linux, "same directory as the exe" is where the default DLL/SO search already looks (no
# extra RPATH plumbing needed), and this puts them where linuxdeploy/macdeployqt will find and
# re-house them in the staged tree those tools actually scan, rather than depending on a
# leftover build-tree RPATH that only happens to still be valid within the same CI job.
if (APPLE)
    install(TARGETS kdchart qt6keychain
            LIBRARY DESTINATION cppwiki.app/Contents/MacOS
    )
else ()
    install(TARGETS kdchart qt6keychain
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION bin
    )
endif ()

# vcpkg's default triplets diverge by platform: x64-linux/x64-osx/arm64-osx default to *static*
# linkage, but x64-windows (used here -- nothing in vcpkg.json/CMakePresets.json/the packaging
# workflow overrides it) defaults to *dynamic*, i.e. real DLLs. That makes spdlog and its
# formatting backend fmt build as spdlog.dll/fmt.dll on Windows only. reflect-cpp's own portfile
# forces itself static-only on Windows (vcpkg_check_linkage(ONLY_STATIC_LIBRARY)), so reflectcpp
# itself never produces a DLL there -- but its JSON backend yyjson carries no such restriction and
# still builds as yyjson.dll. Just like kdchart/qt6keychain above, vcpkg's *-config.cmake files for
# these ports carry no install() rules of their own (they're meant to be consumed from the build
# tree via find_package(), not redistributed), and windeployqt -- unlike linuxdeploy/macdeployqt,
# which do a full recursive dependency scan -- only sweeps Qt's own runtime, not arbitrary
# third-party DLLs. Without this, a packaged Windows installer silently ships a cppwiki.exe that
# fails to start (STATUS_ENTRYPOINT_NOT_FOUND / c0000135) for want of spdlog.dll/fmt.dll/
# yyjson.dll. These are vcpkg IMPORTED targets, not targets this project builds, so
# install(TARGETS) doesn't apply (and would error) -- install(IMPORTED_RUNTIME_ARTIFACTS) is
# CMake's mechanism for co-locating an imported target's runtime DLL the same way. Guarded with
# if(TARGET ...) since fmt::fmt/yyjson::yyjson only exist because spdlog/reflectcpp's own configs
# pull them in transitively (find_dependency()) -- not from an explicit find_package() here.
if (WIN32)
    foreach (cppwiki_vcpkg_dll_target IN ITEMS spdlog::spdlog fmt::fmt yyjson::yyjson)
        if (TARGET ${cppwiki_vcpkg_dll_target})
            install(IMPORTED_RUNTIME_ARTIFACTS ${cppwiki_vcpkg_dll_target}
                    RUNTIME DESTINATION bin
            )
        endif ()
    endforeach ()
endif ()

if (CPPWIKI_ENABLE_CBLITE_STORAGE)
    # CBLite::cblite is a prebuilt binary imported from CPPWIKI_CBLITE_ROOT (see
    # cmake/FindCBLite.cmake), not a target this project builds, so there's no install(TARGETS)
    # to hook. Installing just $<TARGET_FILE:CBLite::cblite> (a single resolved file) is not
    # enough on Linux: the executable's NEEDED entry references the SONAME symlink
    # (e.g. libcblite.so.4), while TARGET_FILE resolves to the fully-versioned real file
    # (libcblite.so.4.0.3) -- linuxdeploy then fails with "Could not find dependency:
    # libcblite.so.4" because that exact name was never installed. Copy the whole runtime
    # directory instead, so the SONAME symlink(s) ship alongside the real file exactly as
    # Couchbase's own release archive lays them out.
    if (APPLE)
        install(DIRECTORY "${CPPWIKI_CBLITE_ROOT}/lib/"
                DESTINATION cppwiki.app/Contents/MacOS
                FILES_MATCHING PATTERN "libcblite*"
        )
    elseif (WIN32)
        install(DIRECTORY "${CPPWIKI_CBLITE_ROOT}/bin/"
                DESTINATION bin
                FILES_MATCHING PATTERN "*.dll"
        )
    else ()
        # Unlike macOS's flat lib/ layout, Couchbase's Linux release nests the actual .so files
        # under a Debian/Ubuntu multiarch triplet subdirectory (lib/x86_64-linux-gnu/), which
        # install(DIRECTORY ... FILES_MATCHING) would preserve as a nested subfolder rather than
        # flattening next to the executable -- glob for the files directly instead so their
        # destination is flat regardless of how deep Couchbase happens to nest them.
        file(GLOB_RECURSE CBLITE_LINUX_RUNTIME_FILES "${CPPWIKI_CBLITE_ROOT}/lib/libcblite.so*")
        if (NOT CBLITE_LINUX_RUNTIME_FILES)
            message(FATAL_ERROR "No libcblite.so* files found under ${CPPWIKI_CBLITE_ROOT}/lib")
        endif ()
        install(FILES ${CBLITE_LINUX_RUNTIME_FILES}
                DESTINATION bin
        )
    endif ()
endif ()

if (CPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP)
    # See ResolveDefaultEditorDistDirectory() in app/program_settings.cc: a packaged install looks
    # for the built editor bundle next to the executable under this exact name.
    if (APPLE)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/frontend/editor/dist/
                DESTINATION cppwiki.app/Contents/MacOS/editor-dist
        )
    else ()
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/frontend/editor/dist/
                DESTINATION bin/editor-dist
        )
    endif ()
endif ()
endif ()

if (CPPWIKI_BUILD_DESKTOP_APP OR CPPWIKI_BUILD_TESTS)
    # Standalone KDGantt-backed Project board Gantt widget (issue #113/#119) — not yet wired into
    # cppwiki_app/MainWindow; see src/gui/project_board/gantt/CMakeLists.txt for its own narrow
    # target(s) and demo executable.
    add_subdirectory(gui/project_board/gantt)
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
            admin/system_keychain_backend.cc
            admin/system_keychain_backend.h
            admin/main.cc
    )

    target_include_directories(cppwiki_admin_cli
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(cppwiki_admin_cli
            PRIVATE
            cpr::cpr
            reflectcpp::reflectcpp
            ftxui::component
            ftxui::dom
            ftxui::screen
            keychain
    )

    set_target_properties(cppwiki_admin_cli
            PROPERTIES
            OUTPUT_NAME cppwiki-admin
    )

    cppwiki_apply_common_options(cppwiki_admin_cli)
endif ()

# Standalone native Kanban board (Project board document kind, issue #113/#114 follow-up) — its
# own isolated target, not yet linked into cppwiki_app. See gui/project_board/kanban/CMakeLists.txt.
if (CPPWIKI_BUILD_DESKTOP_APP)
    add_subdirectory(gui/project_board/kanban)
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
            server/components/ai_config_component.cc
            server/components/ai_config_component.h
            server/components/cppwiki_server_component.cc
            server/components/cppwiki_server_component.h
            server/components/sync_bootstrap_component.cc
            server/components/sync_bootstrap_component.h
            server/handlers/admin_handler.cc
            server/handlers/admin_handler.h
            server/handlers/ai_handler.cc
            server/handlers/ai_handler.h
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
            server/service/ai_chat_service.cc
            server/service/ai_chat_service.h
            server/service/lock_service.cc
            server/service/lock_service.h
            server/service/presence_service.cc
            server/service/presence_service.h
            server/service/sync_gateway_adapter.cc
            server/service/sync_gateway_adapter.h
            server/dto/ai_response.h
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

# Native QTableView Table view for the Project board document kind (issue #117, part of the #113
# umbrella). Standalone/not yet linked into cppwiki_app — see
# gui/project_board/table/CMakeLists.txt. Kept as its own add_subdirectory() block, isolated from
# the rest of this file, so the parallel KDGantt (Gantt) and QML (Kanban) pieces of #113 can each
# add their own equivalent block without touching these lines.
if (CPPWIKI_BUILD_DESKTOP_APP)
    add_subdirectory(gui/project_board/table)
endif ()

# gui/project_board/project_board_native_widget.{cc,h} (compiled directly into cppwiki_app, see
# its entry in the qt_add_executable() call above) hosts all three of the above widgets — link
# them into cppwiki_app now that their add_subdirectory() calls above have actually defined the
# targets (a target_link_libraries() call earlier in this file, before these existed, would fail).
if (CPPWIKI_BUILD_DESKTOP_APP)
    target_link_libraries(cppwiki_app
            PRIVATE
            cppwiki_project_board_gantt
            cppwiki_kanban_board
            cppwiki_project_board_table
    )
endif ()
