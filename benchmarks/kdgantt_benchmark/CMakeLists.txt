add_executable(kdgantt_benchmark
    main.cc
    gantt_diagnostic.cc
    gantt_diagnostic.h
    synthetic_task_model.cc
    synthetic_task_model.h
)

target_link_libraries(kdgantt_benchmark
    PRIVATE
    cppwiki_project_board_gantt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    KDAB::kdchart
)

cppwiki_apply_common_options(kdgantt_benchmark)