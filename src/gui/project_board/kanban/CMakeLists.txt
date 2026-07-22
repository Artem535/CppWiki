# Native Qt/QML Kanban board for the Project board document kind. Standalone piece of the
# native-rendering effort tracked in issue #113: not linked into cppwiki_app yet (see
# demo/main.cc for a way to exercise it on its own). Kept in its own subdirectory/target on
# purpose, isolated from src/CMakeLists.txt's cppwiki_app sources, since the equivalent Gantt
# (KDGantt) and Table (QTableView) pieces are being built the same way in parallel.

add_library(cppwiki_kanban_board STATIC
        kanban_board_document.cc
        kanban_board_document.h
        kanban_board_model.cc
        kanban_board_model.h
        kanban_board_widget.cc
        kanban_board_widget.h
        kanban_column.cc
        kanban_column.h
        kanban_task.cc
        kanban_task.h
        kanban_task_dialog.cc
        kanban_task_dialog.h
)

target_include_directories(cppwiki_kanban_board
        PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_kanban_board
        PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickWidgets
        Qt6::Widgets
)

qt_add_resources(cppwiki_kanban_board cppwiki_kanban_qml_resources
        PREFIX "/cppwiki/kanban"
        BASE qml
        FILES
        qml/KanbanBoard.qml
        qml/KanbanColumnCell.qml
        qml/KanbanCard.qml
)

cppwiki_apply_common_options(cppwiki_kanban_board)

add_executable(cppwiki_kanban_board_demo
        demo/main.cc
)

target_link_libraries(cppwiki_kanban_board_demo
        PRIVATE
        cppwiki_kanban_board
)

cppwiki_apply_common_options(cppwiki_kanban_board_demo)
