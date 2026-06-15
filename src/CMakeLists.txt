qt_add_executable(cppwiki_app
  MANUAL_FINALIZATION
  WIN32
  MACOSX_BUNDLE
    app/application.cc
    bridge/editor_bridge.cc
    bridge/editor_bridge.h
    document/block_note_snapshot.h
    document/document.h
    document/document_validator.cc
    document/document_validator.h
    main.cc
    app/application.h
    app/program_settings.cc
    app/program_settings.h
    core/constants.h
    core/qt_string.h
    gui/i_page.h
    gui/main_window.cc
    gui/main_window.h
    gui/page.cc
    gui/page.h
    storage/cblite_document_repository.h
    storage/local_document_repository.h
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
    Qt6::Widgets
    Qt6::WebChannel
    Qt6::WebEngineWidgets
    spdlog::spdlog
    reflectcpp::reflectcpp
)

if(CPPWIKI_ENABLE_CBLITE_STORAGE)
  target_sources(cppwiki_app
    PRIVATE
      storage/cblite_document_repository.cc
  )

  target_link_libraries(cppwiki_app
    PRIVATE
      CBLite::cblite
  )
endif()

cppwiki_apply_common_options(cppwiki_app)

find_program(CPPWIKI_NPM_EXECUTABLE NAMES npm)
if(CPPWIKI_NPM_EXECUTABLE)
  add_custom_target(editor_bundle
    COMMAND ${CPPWIKI_NPM_EXECUTABLE} ci
    COMMAND ${CPPWIKI_NPM_EXECUTABLE} run build
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/frontend/editor
    COMMENT "Building BlockNote editor bundle"
    VERBATIM
  )

  if(CPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP)
    add_dependencies(cppwiki_app editor_bundle)
  endif()
else()
  message(WARNING "npm was not found; editor_bundle target will be unavailable")
endif()

set_target_properties(cppwiki_app
  PROPERTIES
    OUTPUT_NAME cppwiki
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

qt_finalize_executable(cppwiki_app)
