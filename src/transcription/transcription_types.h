#ifndef CPPWIKI_SRC_TRANSCRIPTION_TRANSCRIPTION_TYPES_H_
#define CPPWIKI_SRC_TRANSCRIPTION_TRANSCRIPTION_TYPES_H_

#include <cstdint>
#include <string>

namespace cppwiki::transcription {

using TrackId = std::string;

enum class TrackRole { Host, Participant };

struct TrackInfo {
  TrackId id;
  TrackRole role = TrackRole::Participant;
  std::string display_name;
};

struct TranscribedPhrase {
  TrackId track_id;
  TrackRole role = TrackRole::Participant;
  std::string speaker_name;
  int64_t start_ms = 0;  // from the start of the call
  int64_t end_ms = 0;
  std::string text;
};

constexpr int kRecognizerSampleRate = 16000;

}  // namespace cppwiki::transcription

#endif  // CPPWIKI_SRC_TRANSCRIPTION_TRANSCRIPTION_TYPES_H_
