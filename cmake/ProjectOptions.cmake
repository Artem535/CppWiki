include_guard(GLOBAL)

function(cppwiki_apply_common_options target_name)
  target_compile_features(${target_name} PUBLIC cxx_std_20)

  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(
      ${target_name}
      PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    )
  endif()

  if(CPPWIKI_ENABLE_CLANG_TIDY)
    find_program(CPPWIKI_CLANG_TIDY_EXE NAMES clang-tidy)
    if(CPPWIKI_CLANG_TIDY_EXE)
      set_target_properties(
        ${target_name}
        PROPERTIES CXX_CLANG_TIDY "${CPPWIKI_CLANG_TIDY_EXE}"
      )
    endif()
  endif()
endfunction()
