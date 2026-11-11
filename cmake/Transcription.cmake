# Local meeting transcription: sherpa-onnx (built from source) plus the GigaAM v3 and Silero
# VAD model files. Ported from Sessio (issue #240). ON by default; set
# CPPWIKI_ENABLE_TRANSCRIPTION=OFF to skip the ~160 MB model download and the sherpa-onnx /
# ONNX Runtime source build entirely (the plain cppwiki_transcription tests do not need them).
option(CPPWIKI_ENABLE_TRANSCRIPTION "Build local meeting transcription (sherpa-onnx + models)" ON)

if (NOT CPPWIKI_ENABLE_TRANSCRIPTION)
    return()
endif ()

include(FetchContent)

# The prebuilt Linux sherpa-onnx tarballs use the old std::string ABI and cannot be used with
# cxx-api.h, so build from source on every platform.
foreach (_opt PYTHON TESTS CHECK PORTAUDIO WEBSOCKET BINARY TTS SPEAKER_DIARIZATION GPU)
    set(SHERPA_ONNX_ENABLE_${_opt} OFF CACHE BOOL "" FORCE)
endforeach ()
set(SHERPA_ONNX_ENABLE_C_API ON CACHE BOOL "" FORCE)
set(SHERPA_ONNX_BUILD_C_API_EXAMPLES OFF CACHE BOOL "" FORCE)
# Match Qt and the x64-windows vcpkg triplet at the DLL boundary.
if (MSVC)
    set(SHERPA_ONNX_USE_STATIC_CRT OFF CACHE BOOL "" FORCE)
endif ()

FetchContent_Declare(sherpa_onnx
        GIT_REPOSITORY https://github.com/k2-fsa/sherpa-onnx.git
        GIT_TAG v1.13.8
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL)

# A normal (non-cache) variable keeps shared libraries scoped to sherpa-onnx.
set(_cppwiki_saved_shared "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS ON)
# sherpa-onnx's sentencepiece dependency still uses std::result_of, which libc++ removes in
# C++20. Keep its dependency tree at its supported C++17; CppWiki's own targets continue to
# use C++20.
set(_cppwiki_saved_cxx_standard "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 17)
# Older dependency projects otherwise let option(BUILD_SHARED_LIBS) replace sherpa's normal
# OFF variable on the first configure. That creates a hidden sentencepiece DLL/dylib with no
# exported symbols instead of a static library.
set(_cppwiki_saved_cmp0077 "${CMAKE_POLICY_DEFAULT_CMP0077}")
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
# The upstream ONNX Runtime x64 downloader checks the Visual Studio platform even with Ninja,
# where CMake leaves that variable empty.
set(_cppwiki_saved_vs_platform "${CMAKE_VS_PLATFORM_NAME}")
if (WIN32 AND MSVC AND NOT CMAKE_VS_PLATFORM_NAME AND
        CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64)$")
    set(CMAKE_VS_PLATFORM_NAME x64)
endif ()
FetchContent_MakeAvailable(sherpa_onnx)
if (_cppwiki_saved_cmp0077)
    set(CMAKE_POLICY_DEFAULT_CMP0077 "${_cppwiki_saved_cmp0077}")
else ()
    unset(CMAKE_POLICY_DEFAULT_CMP0077)
endif ()
set(CMAKE_VS_PLATFORM_NAME "${_cppwiki_saved_vs_platform}")
set(CMAKE_CXX_STANDARD "${_cppwiki_saved_cxx_standard}")
set(BUILD_SHARED_LIBS "${_cppwiki_saved_shared}")
set(CPPWIKI_SHERPA_SOURCE_DIR "${sherpa_onnx_SOURCE_DIR}")

set(CPPWIKI_MODELS_DIR "${CMAKE_BINARY_DIR}/transcription-models" CACHE PATH
        "Where the transcription model files are downloaded to (cache this in CI)")
set(_cppwiki_models_stamp "${CPPWIKI_MODELS_DIR}/.stamp")
add_custom_command(
        OUTPUT "${_cppwiki_models_stamp}"
        COMMAND ${CMAKE_COMMAND} -DMODELS_DIR=${CPPWIKI_MODELS_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/FetchTranscriptionModels.cmake
        COMMAND ${CMAKE_COMMAND} -E touch "${_cppwiki_models_stamp}"
        DEPENDS ${CMAKE_SOURCE_DIR}/cmake/FetchTranscriptionModels.cmake
        COMMENT "Fetching transcription models"
        VERBATIM)
add_custom_target(cppwiki_transcription_models ALL DEPENDS "${_cppwiki_models_stamp}")

# Directory of ONNX Runtime fetched by sherpa-onnx.
FetchContent_GetProperties(onnxruntime SOURCE_DIR _cppwiki_ort_dir)
set(CPPWIKI_ONNXRUNTIME_LIB_DIR "${_cppwiki_ort_dir}/lib")

# ONNX Runtime: libonnxruntime.so* (Linux), libonnxruntime*.dylib (macOS), onnxruntime*.dll
# (Windows; the DLL sits in lib/ or ../bin of the archive).
file(GLOB CPPWIKI_ONNXRUNTIME_LIBS
        "${CPPWIKI_ONNXRUNTIME_LIB_DIR}/libonnxruntime*.so*"
        "${CPPWIKI_ONNXRUNTIME_LIB_DIR}/libonnxruntime*.dylib"
        "${CPPWIKI_ONNXRUNTIME_LIB_DIR}/onnxruntime*.dll"
        "${CPPWIKI_ONNXRUNTIME_LIB_DIR}/../bin/onnxruntime*.dll")
if (NOT CPPWIKI_ONNXRUNTIME_LIBS)
    message(FATAL_ERROR
            "ONNX Runtime libraries not found. Searched: "
            "${CPPWIKI_ONNXRUNTIME_LIB_DIR} and ${CPPWIKI_ONNXRUNTIME_LIB_DIR}/../bin "
            "(patterns libonnxruntime*.so*, libonnxruntime*.dylib, onnxruntime*.dll). "
            "sherpa-onnx may have changed where it unpacks ONNX Runtime.")
endif ()
