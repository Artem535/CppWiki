#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "sherpa-onnx/c-api/cxx-api.h"
#include "transcription/model_locator.h"
#include "transcription/sherpa_backend.h"
#include "transcription/transcription_engine.h"

namespace fs = std::filesystem;
using namespace cppwiki::transcription;
using namespace std::chrono_literals;

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    std::exit(EXIT_FAILURE);
  }
}

std::optional<ModelPaths> models() {
  const char* env = std::getenv("CPPWIKI_MODELS_DIR");
  return locateModels(fs::current_path(), currentPlatform(), env ? env : "").paths;
}

std::vector<int16_t> loadSample() {
  const char* env = std::getenv("CPPWIKI_MODELS_DIR");
  const fs::path wav = fs::path(env ? env : "") / "gigaam-v3-rnnt" / "test_wavs" / "example.wav";
  auto wave = sherpa_onnx::cxx::ReadWave(wav.string());
  std::vector<int16_t> out(wave.samples.size());
  for (size_t i = 0; i < out.size(); ++i)
    out[i] = static_cast<int16_t>(wave.samples[i] * 32767.0F);
  return out;  // 16 kHz
}

bool hasCyrillic(const std::string& text) {
  for (size_t i = 0; i + 1 < text.size(); ++i) {
    const auto c = static_cast<unsigned char>(text[i]);
    if (c == 0xD0 || c == 0xD1)
      return true;
  }
  return false;
}

struct PhraseLag {
  TranscribedPhrase phrase;
  double lag_s;
};

struct RunResult {
  std::vector<PhraseLag> phrases;
  EngineStats stats;
};

double durationS(const TranscribedPhrase& p) {
  return static_cast<double>(p.end_ms - p.start_ms) / 1000.0;
}

// Feeds `tracks` in real time, 100 ms at a time. Track i < speakers_start_s.size() plays
// the sample starting `speakers_start_s[i]` seconds into the stream (silence before that);
// every other track is silent. Engine start offsets stay 0, so end_ms already contains the
// speaker's delay and lag = wall time of delivery - end_ms.
RunResult runRealtime(const ModelPaths& paths, int tracks,
                      const std::vector<double>& speakers_start_s,
                      const std::vector<int16_t>& sample) {
  RunResult result;
  std::mutex mutex;
  EngineCallbacks cb;
  const auto start = std::chrono::steady_clock::now();
  cb.on_phrase = [&](const TranscribedPhrase& p) {
    const double audio_end = static_cast<double>(p.end_ms) / 1000.0;
    const double now =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::lock_guard lock(mutex);
    result.phrases.push_back({p, now - audio_end});
  };
  TranscriptionEngine engine(makeSherpaVadFactory(paths), makeSherpaRecognizer(paths), cb);
  for (int t = 0; t < tracks; ++t) {
    engine.addTrack({"t" + std::to_string(t), t == 0 ? TrackRole::Host : TrackRole::Participant,
                     "N" + std::to_string(t)},
                    0);
  }
  const size_t step = 1600;  // 100 ms at 16 kHz
  const std::vector<int16_t> quiet(step, 0);
  double lastEndS = 0;
  for (double s : speakers_start_s)
    lastEndS = std::max(lastEndS, s);
  const size_t total =
      static_cast<size_t>(lastEndS * 16000) + sample.size() + 16000 * 2;  // two seconds of tail
  for (size_t pos = 0; pos < total; pos += step) {
    for (int t = 0; t < tracks; ++t) {
      const std::string id = "t" + std::to_string(t);
      bool fed = false;
      if (static_cast<size_t>(t) < speakers_start_s.size()) {
        const size_t begin = static_cast<size_t>(speakers_start_s[static_cast<size_t>(t)] * 16000);
        if (pos >= begin && pos - begin < sample.size()) {
          const size_t n = std::min(step, sample.size() - (pos - begin));
          engine.pushAudio(id, sample.data() + (pos - begin), n, 16000);
          fed = true;
        }
      }
      if (!fed)
        engine.pushAudio(id, quiet.data(), step, 16000);
    }
    std::this_thread::sleep_until(start + std::chrono::milliseconds((pos + step) / 16));
  }
  engine.stop(10s);
  result.stats = engine.stats();
  std::lock_guard lock(mutex);
  return result;
}

void printPhrases(const RunResult& r) {
  for (const auto& pl : r.phrases) {
    std::cout << "[loadtest] track=" << pl.phrase.track_id << " duration=" << durationS(pl.phrase)
              << " s lag=" << pl.lag_s << " s" << std::endl;
  }
}

void TestRecognisesRussianSample(const ModelPaths& paths) {
  auto recognizer = makeSherpaRecognizer(paths);
  const auto sample = loadSample();
  std::vector<float> floats(sample.size());
  for (size_t i = 0; i < sample.size(); ++i)
    floats[i] = static_cast<float>(sample[i]) / 32768.0F;
  const std::string text = recognizer->transcribe(floats);
  Require(!text.empty(), "the sample must decode to non-empty text");
  Require(hasCyrillic(text), "text had no Cyrillic letters");
}

void TestVadRetainsAudioBeforeAndAfterSpeechBoundaries(const ModelPaths& paths) {
  std::vector<float> audio(16000, 0.0F);
  for (auto sample : loadSample())
    audio.push_back(sample / 32768.0F);
  audio.resize(audio.size() + 16000, 0.0F);
  sherpa_onnx::cxx::VadModelConfig config;
  config.silero_vad.model = paths.vad.string();
  config.silero_vad.threshold = 0.5F;
  config.silero_vad.min_silence_duration = 0.8F;
  config.silero_vad.min_speech_duration = 0.25F;
  config.silero_vad.max_speech_duration = 20.0F;
  config.silero_vad.window_size = 512;
  config.sample_rate = 16000;
  auto reference = sherpa_onnx::cxx::VoiceActivityDetector::Create(config, 120.0F);
  auto actual = makeSherpaVadFactory(paths)();
  std::vector<SpeechSegment> segments;
  for (size_t offset = 0; offset + 512 <= audio.size(); offset += 512) {
    reference.AcceptWaveform(audio.data() + offset, 512);
    actual->accept(audio.data() + offset, 512);
    while (actual->hasSegment())
      segments.push_back(actual->popSegment());
  }
  reference.Flush();
  actual->flush();
  while (actual->hasSegment())
    segments.push_back(actual->popSegment());
  Require(!segments.empty(), "the VAD must close at least one phrase");
  Require(segments.size() == 1, "the source's short internal pause stays within one phrase");
  Require(!reference.IsEmpty(), "the reference VAD must see the phrase too");
  const auto raw = reference.Front();
  Require(segments.front().start_sample <= raw.start - 1600,
          "at least 100 ms of leading context is retained");
  Require(segments.front().start_sample + static_cast<int64_t>(segments.front().samples.size()) >=
              raw.start + static_cast<int64_t>(raw.samples.size()) + 2400,
          "at least 150 ms of trailing context is retained");
  const auto& first = segments.front();
  Require(first.start_sample >= 0, "segment start is non-negative");
  Require(first.start_sample + static_cast<int64_t>(first.samples.size()) <=
              static_cast<int64_t>(audio.size()),
          "segment stays inside the fed audio");
  Require(
      std::equal(first.samples.begin(), first.samples.end(), audio.begin() + first.start_sample),
      "retained samples must be a verbatim copy of the source audio");
  for (size_t i = 1; i < segments.size(); ++i) {
    Require(segments[i].start_sample >=
                segments[i - 1].start_sample + static_cast<int64_t>(segments[i - 1].samples.size()),
            "segments must not overlap");
  }
}

void TestEngineProducesTimedPhrasesFromSample(const ModelPaths& paths) {
  const auto sample = loadSample();
  std::mutex m;
  std::vector<TranscribedPhrase> phrases;
  EngineCallbacks cb;
  cb.on_phrase = [&](const TranscribedPhrase& p) {
    std::lock_guard lock(m);
    phrases.push_back(p);
  };
  TranscriptionEngine engine(makeSherpaVadFactory(paths), makeSherpaRecognizer(paths), cb);
  engine.addTrack({"a", TrackRole::Participant, "A"}, 5000);
  engine.pushAudio("a", sample.data(), sample.size(), 16000);
  engine.removeTrack("a");
  engine.stop(15s);
  Require(!phrases.empty(), "the engine must emit phrases for the sample");
  for (const auto& p : phrases) {
    Require(p.start_ms >= 5000, "phrase timestamps include the track offset");
    Require(p.end_ms > p.start_ms, "phrase timestamps are ordered");
    Require(hasCyrillic(p.text), "phrase text must be Cyrillic");
  }
}

void TestTwoStaggeredSpeakersAmongTenTracksKeepLagProportionalToPhraseLength(
    const ModelPaths& paths) {
  const auto result = runRealtime(paths, 10, {0.0, 4.0}, loadSample());
  printPhrases(result);
  Require(result.phrases.size() >= 2, "at least one phrase per speaker");
  for (const auto& pl : result.phrases) {
    Require(pl.lag_s <= 0.8 + 0.15 * durationS(pl.phrase) + 0.2,
            "track " + pl.phrase.track_id + " phrase of " + std::to_string(durationS(pl.phrase)) +
                " s lagged beyond the bound");
  }
  Require(result.stats.queued == 0, "the queue must drain");
  Require(result.stats.dropped_on_stop == 0, "nothing may be dropped");
}

// Worst case, documented without a tight bound: two identical long phrases that end at
// the same instant are decoded one after the other by design (one shared recogniser), so
// the second one waits for the first. This only guards against unbounded growth.
void TestTwoSimultaneousSpeakersAmongTenTracksDrainWithinThreeSeconds(const ModelPaths& paths) {
  const auto result = runRealtime(paths, 10, {0.0, 0.0}, loadSample());
  printPhrases(result);
  Require(result.phrases.size() >= 4, "both speakers produce phrases");
  for (const auto& pl : result.phrases) {
    Require(pl.lag_s <= 3.0, "simultaneous phrases must drain within three seconds");
  }
  Require(result.stats.queued == 0, "the queue must drain");
  Require(result.stats.dropped_on_stop == 0, "nothing may be dropped");
}

}  // namespace

int main() {
  const auto paths = models();
  if (!paths) {
    // Matching the Sessio suite's GTEST_SKIP semantics: absent models are a skip, not a
    // failure — set CPPWIKI_MODELS_DIR (the build does this for ctest automatically).
    std::printf(
        "cppwiki_transcription_model_tests: SKIPPED (models not installed; set "
        "CPPWIKI_MODELS_DIR)\n");
    return EXIT_SUCCESS;
  }

  TestRecognisesRussianSample(*paths);
  TestVadRetainsAudioBeforeAndAfterSpeechBoundaries(*paths);
  TestEngineProducesTimedPhrasesFromSample(*paths);

  if (std::getenv("CPPWIKI_TRANSCRIPTION_LOADTEST") != nullptr) {
    TestTwoStaggeredSpeakersAmongTenTracksKeepLagProportionalToPhraseLength(*paths);
    TestTwoSimultaneousSpeakersAmongTenTracksDrainWithinThreeSeconds(*paths);
  }

  std::printf("cppwiki_transcription_model_tests: all tests passed\n");
  return EXIT_SUCCESS;
}
