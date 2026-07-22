# Standalone Project board Gantt widget backed by KDGantt (see issue #113/#119). Deliberately its
# own narrow target/subdirectory: two other agents are adding the equivalent Kanban (QML) and
# Table (QTableView) pieces of #113 in parallel, each touching the same shared CMakeLists.txt
# files — keeping this to a single add_subdirectory() call there (see src/CMakeLists.txt) and all
# the actual target definitions here minimizes line-level merge conflicts between the three PRs.

add_library(cppwiki_project_board_gantt STATIC
        project_board_gantt_critical_path.cc
        project_board_gantt_critical_path.h
        project_board_gantt_item_delegate.cc
        project_board_gantt_item_delegate.h
        project_board_gantt_model.cc
        project_board_gantt_model.h
        project_board_gantt_widget.cc
        project_board_gantt_widget.h
)

target_include_directories(cppwiki_project_board_gantt
        PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_project_board_gantt
        PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
        KDAB::kdchart
)

cppwiki_apply_common_options(cppwiki_project_board_gantt)

# Demo executable: loads a small sample board (summary task with subtasks, a milestone, and one
# dependency link) into the widget and shows it — see demo/main.cc for what to look for when
# running it manually (`./cppwiki_project_board_gantt_demo`), since there's no automated
# way to verify rendering/visual behavior.
add_executable(cppwiki_project_board_gantt_demo
        demo/main.cc
)

target_link_libraries(cppwiki_project_board_gantt_demo
        PRIVATE
        cppwiki_project_board_gantt
)

cppwiki_apply_common_options(cppwiki_project_board_gantt_demo)
