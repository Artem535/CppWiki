# Native QTableView-based Table view for the Project board document kind (issue #117, part of the
# #113 umbrella). Standalone: not yet linked into cppwiki_app — see demo_main.cc for a way to run
# it on its own. Kept as its own isolated add_subdirectory() (rather than folding these sources
# into cppwiki_app's own source list in ../../../CMakeLists.txt) so the parallel KDGantt/QML pieces
# of #113 don't need to touch the same lines of that file.
add_library(cppwiki_project_board_table STATIC
        project_task.cc
        project_task.h
        project_task_table_model.cc
        project_task_table_model.h
        project_pill_paint.cc
        project_pill_paint.h
        project_status_pill_delegate.cc
        project_status_pill_delegate.h
        project_priority_pill_delegate.cc
        project_priority_pill_delegate.h
        project_date_edit_delegate.cc
        project_date_edit_delegate.h
        project_numeric_spin_delegate.cc
        project_numeric_spin_delegate.h
        project_task_table_widget.cc
        project_task_table_widget.h
)

target_include_directories(cppwiki_project_board_table
        PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_project_board_table
        PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

cppwiki_apply_common_options(cppwiki_project_board_table)

add_executable(cppwiki_project_board_table_demo
        demo_main.cc
)

target_link_libraries(cppwiki_project_board_table_demo
        PRIVATE
        cppwiki_project_board_table
)

cppwiki_apply_common_options(cppwiki_project_board_table_demo)
