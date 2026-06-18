add_executable(cppwiki_server
  server_main.cc
  server_application.cc
  server_application.h
  server_config.h
  controllers/health_controller.cc
  controllers/health_controller.h
  controllers/auth_controller.cc
  controllers/auth_controller.h
  controllers/lock_controller.cc
  controllers/lock_controller.h
  controllers/presence_controller.cc
  controllers/presence_controller.h
  filters/jwt_auth_filter.cc
  filters/jwt_auth_filter.h
  dto/api_response.h
  dto/health_response.h
  dto/auth_responses.h
  dto/lock_responses.h
  dto/presence_responses.h
  logging/logger.cc
  logging/logger.h
)

target_include_directories(cppwiki_server
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(cppwiki_server
  PRIVATE
    Drogon::Drogon
    spdlog::spdlog
    reflectcpp::reflectcpp
)

cppwiki_apply_common_options(cppwiki_server)

set_target_properties(cppwiki_server
  PROPERTIES
    OUTPUT_NAME cppwiki-server
)
