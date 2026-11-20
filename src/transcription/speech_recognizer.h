#ifndef CPPWIKI_SRC_TRANSCRIPTION_SPEECH_RECOGNIZER_H_
#define CPPWIKI_SRC_TRANSCRIPTION_SPEECH_RECOGNIZER_H_

#include <span>
#include <string>

namespace cppwiki::transcription {

// Turns one finished phrase (16 kHz float mono) into text. Throws
// std::runtime_error when decoding fails. Called from the decode worker only.
class ISpeechRecognizer {
 public:
  virtual ~ISpeechRecognizer() = default;
  virtual std::string transcribe(std::span<const float> samples) = 0;
};

}  // namespace cppwiki::transcription

#endif  // CPPWIKI_SRC_TRANSCRIPTION_SPEECH_RECOGNIZER_H_
