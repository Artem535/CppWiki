#ifndef CPPWIKI_SRC_TRANSCRIPTION_PHRASE_SEGMENTER_H_
#define CPPWIKI_SRC_TRANSCRIPTION_PHRASE_SEGMENTER_H_

#include <memory>
#include <span>
#include <vector>

#include "transcription/voice_activity.h"

namespace cppwiki::transcription {

// Per-track: cuts a 16 kHz stream into fixed windows for the VAD and collects
// the phrases it closes.
class PhraseSegmenter {
 public:
  explicit PhraseSegmenter(std::unique_ptr<IVoiceActivityDetector> vad);

  std::vector<SpeechSegment> feed(std::span<const float> samples);
  // End of stream: pad the partial window with zeros, close an open phrase.
  std::vector<SpeechSegment> flush();

 private:
  std::vector<SpeechSegment> drain();

  std::unique_ptr<IVoiceActivityDetector> vad_;
  std::vector<float> pending_;
};

}  // namespace cppwiki::transcription

#endif  // CPPWIKI_SRC_TRANSCRIPTION_PHRASE_SEGMENTER_H_
