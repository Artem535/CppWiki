#ifndef CPPWIKI_SRC_TRANSCRIPTION_SHERPA_BACKEND_H_
#define CPPWIKI_SRC_TRANSCRIPTION_SHERPA_BACKEND_H_

#include <memory>

#include "transcription/model_locator.h"
#include "transcription/speech_recognizer.h"
#include "transcription/transcription_engine.h"
#include "transcription/transcription_tuning.h"

namespace cppwiki::transcription {

std::shared_ptr<ISpeechRecognizer> makeSherpaRecognizer(const ModelPaths& paths,
                                                        int num_threads = 4);

TranscriptionEngine::VadFactory makeSherpaVadFactory(const ModelPaths& paths,
                                                     const TranscriptionTuning& tuning = {});

}  // namespace cppwiki::transcription

#endif  // CPPWIKI_SRC_TRANSCRIPTION_SHERPA_BACKEND_H_
