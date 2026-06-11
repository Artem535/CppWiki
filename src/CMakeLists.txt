qt_add_executable(cppwiki_app
  WIN32
  MACOSX_BUNDLE
)

target_sources(cppwiki_app
  PRIVATE
    main.cc
    app/application.cc
    app/application.h
    gui/i_page.h
    gui/main_window.cc
    gui/main_window.h
    gui/page.cc
    gui/page.h
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
)

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
