# Local meeting-transcription engine core ported from Sessio (issue #240). Two narrow targets:
# cppwiki_transcription is engine-agnostic (pure C++20 stdlib, no sherpa-onnx dependency), so
# the engine/segmenter/resampler/locator tests run with no models and no sherpa build;
# cppwiki_transcription_sherpa is the real backend behind the ISpeechRecognizer /
# IVoiceActivityDetector seams and is the only target that links sherpa-onnx. Not yet linked
# into cppwiki_app — the call UI and transcript persistence land in later tasks.

find_package(Threads REQUIRED)

add_library(cppwiki_transcription STATIC
        model_locator.cc
        model_locator.h
        phrase_segmenter.cc
        phrase_segmenter.h
        resampler.cc
        resampler.h
        speech_recognizer.h
        transcription_engine.cc
        transcription_engine.h
        transcription_tuning.h
        transcription_types.h
        voice_activity.h
)

target_include_directories(cppwiki_transcription
        PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(cppwiki_transcription
        PUBLIC
        Threads::Threads
)

cppwiki_apply_common_options(cppwiki_transcription)

add_library(cppwiki_transcription_sherpa STATIC
        sherpa_backend.cc
        sherpa_backend.h
)

target_include_directories(cppwiki_transcription_sherpa
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CPPWIKI_SHERPA_SOURCE_DIR}
)

target_link_libraries(cppwiki_transcription_sherpa
        PUBLIC
        cppwiki_transcription
        PRIVATE
        sherpa-onnx-cxx-api
)

cppwiki_apply_common_options(cppwiki_transcription_sherpa)
