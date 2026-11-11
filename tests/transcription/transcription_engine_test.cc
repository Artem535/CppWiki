#include "transcription/transcription_engine.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "tests/transcription/transcription_test_support.h"

namespace {

using namespace cppwiki::transcription;
using namespace cppwiki::transcription::testing;
using namespace std::chrono_literals;

void Require(bool condition, std::string_view message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", std::string(message).c_str());
    std::exit(EXIT_FAILURE);
  }
}

struct Collector {
  void add(const TranscribedPhrase& p) {
    std::lock_guard lock(mutex);
    phrases.push_back(p);
  }
  size_t size() {
    std::lock_guard lock(mutex);
    return phrases.size();
  }
  std::vector<TranscribedPhrase> snapshot() {
    std::lock_guard lock(mutex);
    return phrases;
  }
  std::mutex mutex;
  std::vector<TranscribedPhrase> phrases;
  std::mutex delayedMutex;
  std::vector<bool> delayed;
  void addDelayed(bool d) {
    std::lock_guard lock(delayedMutex);
    delayed.push_back(d);
  }
  bool sawDelayed(bool value) {
    std::lock_guard lock(delayedMutex);
    for (bool d : delayed)
      if (d == value)
        return true;
    return false;
  }
};

std::unique_ptr<TranscriptionEngine> makeEngine(Collector& c,
                                                std::shared_ptr<ISpeechRecognizer> rec,
                                                EngineConfig config = {}) {
  EngineCallbacks cb;
  cb.on_phrase = [&c](const TranscribedPhrase& p) { c.add(p); };
  cb.on_delayed = [&c](bool d) { c.addDelayed(d); };
  return std::make_unique<TranscriptionEngine>([] { return std::make_unique<FakeVad>(); },
                                               std::move(rec), cb, config);
}

void push(TranscriptionEngine& e, const TrackId& id, const std::vector<int16_t>& audio) {
  e.pushAudio(id, audio.data(), audio.size(), 48000);
}

class ThrowingVad : public FakeVad {
 public:
  void accept(const float*, size_t) override {
    throw std::runtime_error("vad boom");
  }
};

void TestEmitsPhraseWithRoleNameAndTimes() {
  Collector c;
  auto engine = makeEngine(c, std::make_shared<FakeRecognizer>());
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 1000);
  push(*engine, "alice", concat({silence(500), speech(1000), silence(800)}));

  Require(waitFor([&] { return c.size() == 1; }), "one phrase must arrive");
  const auto p = c.snapshot().front();
  Require(p.track_id == "alice", "track id");
  Require(p.role == TrackRole::Participant, "role");
  Require(p.speaker_name == "Alice", "speaker name");
  Require(p.text == "phrase 0", "recognizer text");
  Require(std::abs(static_cast<double>(p.start_ms) - 1500.0) <= 64.0, "start_ms ~1500");
  Require(std::abs(static_cast<double>(p.end_ms) - 2500.0) <= 64.0, "end_ms ~2500");
}

void TestSeparatesTracksAndKeepsDecodeOrder() {
  Collector c;
  auto engine = makeEngine(c, std::make_shared<FakeRecognizer>());
  engine->addTrack({"me", TrackRole::Host, "Dr"}, 0);
  engine->addTrack({"client", TrackRole::Participant, "C"}, 0);
  push(*engine, "me", concat({speech(500), silence(600)}));
  push(*engine, "client", concat({speech(500), silence(600)}));

  Require(waitFor([&] { return c.size() == 2; }), "two phrases must arrive");
  const auto phrases = c.snapshot();
  Require(phrases[0].track_id != phrases[1].track_id, "phrases must come from both tracks");
  Require(phrases[0].text == "phrase 0", "one worker decodes in queue order");
  Require(phrases[1].text == "phrase 1", "decode order follows queue order");
}

void TestRemoveTrackFlushesOpenPhrase() {
  Collector c;
  auto engine = makeEngine(c, std::make_shared<FakeRecognizer>());
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  push(*engine, "alice", speech(1000));  // no trailing silence: phrase still open
  engine->removeTrack("alice");
  Require(waitFor([&] { return c.size() == 1; }), "removeTrack must flush the open phrase");
}

void TestResetSeparatesSpeechAndPreservesRemoteTrack() {
  Collector c;
  auto engine = makeEngine(c, std::make_shared<FakeRecognizer>());
  engine->addTrack({"local", TrackRole::Host, "Local"}, 0);
  engine->addTrack({"remote", TrackRole::Participant, "Remote"}, 0);
  push(*engine, "local", speech(500));
  push(*engine, "remote", speech(500));
  engine->resetTrack("local", 20000);
  push(*engine, "local", concat({speech(500), silence(200)}));
  push(*engine, "remote", concat({speech(500), silence(200)}));
  engine->stop(2s);
  const auto phrases = c.snapshot();
  Require(phrases.size() == 3, "three phrases: pre-reset local, post-reset local, remote");
  std::vector<TranscribedPhrase> local;
  std::vector<TranscribedPhrase> remote;
  for (const auto& phrase : phrases)
    (phrase.track_id == "local" ? local : remote).push_back(phrase);
  Require(local.size() == 2, "two local phrases around the reset");
  Require(local[0].end_ms < 20000, "pre-reset phrase ends before the reset offset");
  Require(local[1].start_ms >= 20000, "post-reset phrase starts at the reset offset");
  Require(remote.size() == 1, "the remote track survives the local reset");
  Require(remote[0].end_ms - remote[0].start_ms > 900, "remote phrase spans both pushes");
}

void TestResetAcceptsReplacementSampleRateAndKeepsIdentity() {
  Collector c;
  auto engine = makeEngine(c, std::make_shared<FakeRecognizer>());
  engine->addTrack({"local", TrackRole::Host, "Local"}, 500);
  push(*engine, "local", speech(500));
  engine->resetTrack("local", 10000);
  std::vector<int16_t> replacement(8000, 16000);
  replacement.resize(11200, 0);
  engine->pushAudio("local", replacement.data(), replacement.size(), 16000);
  engine->stop(2s);
  const auto phrases = c.snapshot();
  Require(phrases.size() == 2, "phrases before and after the rate change");
  Require(phrases[1].track_id == "local", "track id survives the reset");
  Require(phrases[1].speaker_name == "Local", "speaker name survives the reset");
  Require(phrases[1].start_ms >= 10000, "post-reset phrase uses the new offset");
}

void TestIgnoresUnknownAndRemovedTracks() {
  Collector c;
  auto engine = makeEngine(c, std::make_shared<FakeRecognizer>());
  push(*engine, "nobody", speech(500));
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  engine->removeTrack("alice");
  push(*engine, "alice", concat({speech(500), silence(600)}));
  std::this_thread::sleep_for(150ms);
  Require(c.size() == 0, "unknown and removed tracks must be ignored");
}

void TestStopDrainsQueuedPhrases() {
  Collector c;
  auto rec = std::make_shared<FakeRecognizer>([](int, size_t) {
    std::this_thread::sleep_for(20ms);
    return std::string("ok");
  });
  auto engine = makeEngine(c, rec);
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  for (int i = 0; i < 5; ++i)
    push(*engine, "alice", concat({speech(200), silence(200)}));
  engine->stop(2s);
  Require(c.size() == 5, "stop must drain all queued phrases");
  Require(engine->stats().dropped_on_stop == 0, "nothing is dropped with a full drain window");
}

void TestStopWithZeroTimeoutCountsDroppedPhrases() {
  Collector c;
  auto rec = std::make_shared<FakeRecognizer>([](int, size_t) {
    std::this_thread::sleep_for(100ms);
    return std::string("slow");
  });
  auto engine = makeEngine(c, rec);
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  for (int i = 0; i < 6; ++i)
    push(*engine, "alice", concat({speech(200), silence(200)}));
  engine->stop(0ms);
  const auto stats = engine->stats();
  Require(c.size() + stats.dropped_on_stop == 6,
          "emitted + dropped must cover every queued phrase");
  Require(stats.dropped_on_stop > 0, "a zero timeout must drop some phrases");
}

void TestDecodeFailureIsCountedAndSessionContinues() {
  Collector c;
  auto rec = std::make_shared<FakeRecognizer>([](int call, size_t) -> std::string {
    if (call == 1)
      throw std::runtime_error("boom");
    return "ok";
  });
  auto engine = makeEngine(c, rec);
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  for (int i = 0; i < 3; ++i)
    push(*engine, "alice", concat({speech(300), silence(300)}));
  engine->stop(2s);
  Require(c.size() == 2, "a decode failure must not end the session");
  Require(engine->stats().decode_failures == 1, "the decode failure is counted");
}

void TestEmptyTextIsNotEmitted() {
  Collector c;
  auto rec = std::make_shared<FakeRecognizer>([](int, size_t) { return std::string("  "); });
  auto engine = makeEngine(c, rec);
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  push(*engine, "alice", concat({speech(300), silence(300)}));
  engine->stop(2s);
  Require(c.size() == 0, "whitespace-only decode results must not be emitted");
}

void TestReportsDelayWhenQueueFallsBehindAndRecovers() {
  Collector c;
  auto rec = std::make_shared<FakeRecognizer>([](int, size_t) {
    std::this_thread::sleep_for(120ms);
    return std::string("slow");
  });
  EngineConfig config;
  config.delayed_after = 50ms;
  auto engine = makeEngine(c, rec, config);
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  for (int i = 0; i < 3; ++i)
    push(*engine, "alice", concat({speech(200), silence(200)}));
  Require(waitFor([&] { return c.size() == 3; }), "all phrases must arrive");
  Require(c.sawDelayed(true), "the delayed flag must be reported when the queue falls behind");

  push(*engine, "alice", concat({speech(200), silence(200)}));
  Require(waitFor([&] { return c.size() == 4; }), "the fourth phrase must arrive");
  Require(c.sawDelayed(false), "the delayed flag must clear on recovery");
  Require(!engine->stats().delayed, "stats report the queue as caught up");
}

void TestTenTracksTwoSpeakingAllPhrasesArriveAndQueueDrains() {
  Collector c;
  auto engine = makeEngine(c, std::make_shared<FakeRecognizer>());
  for (int i = 0; i < 10; ++i) {
    engine->addTrack({"t" + std::to_string(i), i == 0 ? TrackRole::Host : TrackRole::Participant,
                      "N" + std::to_string(i)},
                     0);
  }
  for (int round = 0; round < 3; ++round) {
    for (int i = 0; i < 10; ++i) {
      const bool speaks = i < 2;
      push(*engine, "t" + std::to_string(i),
           speaks ? concat({speech(300), silence(300)}) : silence(600));
    }
  }
  engine->stop(2s);
  Require(c.size() == 6, "two speaking tracks over three rounds produce six phrases");
  Require(engine->stats().queued == 0, "the queue must drain");
}

void TestPushAfterStopIsIgnored() {
  Collector c;
  auto engine = makeEngine(c, std::make_shared<FakeRecognizer>());
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  engine->stop(1s);
  push(*engine, "alice", concat({speech(300), silence(300)}));
  std::this_thread::sleep_for(100ms);
  Require(c.size() == 0, "audio pushed after stop must be ignored");
}

void TestThrowingPhraseCallbackKeepsEngineRunningAndIsNotADecodeFailure() {
  std::atomic<int> calls{0};
  EngineCallbacks cb;
  cb.on_phrase = [&](const TranscribedPhrase&) {
    ++calls;
    throw std::runtime_error("callback boom");
  };
  TranscriptionEngine engine([] { return std::make_unique<FakeVad>(); },
                             std::make_shared<FakeRecognizer>(), cb);
  engine.addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  for (int i = 0; i < 3; ++i)
    push(engine, "alice", concat({speech(300), silence(300)}));
  engine.stop(2s);
  Require(calls.load() == 3, "the phrase callback was attempted for every phrase");
  Require(engine.stats().decode_failures == 0, "a throwing callback is not a decode failure");
  Require(engine.stats().phrases == 0, "failed callbacks do not count as emitted phrases");
  Require(engine.stats().callback_failures == 3, "callback failures are counted");
}

void TestDelayedFlagClearsWhenQueueDrainsWithoutNewPhrase() {
  Collector c;
  auto rec = std::make_shared<FakeRecognizer>([](int, size_t) {
    std::this_thread::sleep_for(120ms);
    return std::string("slow");
  });
  EngineConfig config;
  config.delayed_after = 50ms;
  auto engine = makeEngine(c, rec, config);
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  for (int i = 0; i < 3; ++i)
    push(*engine, "alice", concat({speech(200), silence(200)}));
  Require(waitFor([&] { return c.sawDelayed(true); }), "delayed must be reported");
  Require(waitFor([&] { return c.size() == 3; }), "all phrases must arrive");
  Require(waitFor([&] { return c.sawDelayed(false); }), "delayed must clear once drained");
  Require(!engine->stats().delayed, "stats report the queue as caught up");
}

void TestThrowingDelayedCallbackDoesNotCrashAndPhrasesStillArrive() {
  Collector c;
  auto rec = std::make_shared<FakeRecognizer>([](int, size_t) {
    std::this_thread::sleep_for(120ms);
    return std::string("slow");
  });
  EngineConfig config;
  config.delayed_after = 50ms;
  EngineCallbacks cb;
  cb.on_phrase = [&c](const TranscribedPhrase& p) { c.add(p); };
  std::atomic<int> delayedCalls{0};
  cb.on_delayed = [&delayedCalls](bool) {
    ++delayedCalls;
    throw std::runtime_error("delayed boom");
  };
  TranscriptionEngine engine([] { return std::make_unique<FakeVad>(); }, rec, cb, config);
  engine.addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  for (int i = 0; i < 3; ++i)
    push(engine, "alice", concat({speech(200), silence(200)}));
  Require(waitFor([&] { return c.size() == 3; }), "phrases still arrive");
  Require(waitFor([&] { return delayedCalls.load() > 0; }), "the delayed callback ran");
  Require(waitFor([&] { return engine.stats().callback_failures > 0; }),
          "a throwing delayed callback is counted");
}

void TestFailingTrackIsIsolatedAndCounted() {
  Collector c;
  EngineCallbacks cb;
  cb.on_phrase = [&c](const TranscribedPhrase& p) { c.add(p); };
  std::atomic<int> made{0};
  TranscriptionEngine engine(
      [&]() -> std::unique_ptr<IVoiceActivityDetector> {
        if (made++ == 0)
          return std::make_unique<ThrowingVad>();
        return std::make_unique<FakeVad>();
      },
      std::make_shared<FakeRecognizer>(), cb);
  engine.addTrack({"bad", TrackRole::Participant, "Bad"}, 0);
  engine.addTrack({"good", TrackRole::Participant, "Good"}, 0);
  push(engine, "bad", concat({speech(500), silence(600)}));
  push(engine, "bad", concat({speech(500), silence(600)}));
  push(engine, "good", concat({speech(500), silence(600)}));
  Require(waitFor([&] { return c.size() == 1; }), "only the healthy track emits");
  engine.stop(2s);
  Require(c.snapshot().front().track_id == "good", "the emitted phrase is from the good track");
  Require(engine.stats().track_failures == 1, "the failing track is counted once");
}

void TestNonStdExceptionFromRecogniserIsCountedAsFailure() {
  Collector c;
  auto rec = std::make_shared<FakeRecognizer>([](int call, size_t) -> std::string {
    if (call == 0)
      throw 42;
    return "ok";
  });
  auto engine = makeEngine(c, rec);
  engine->addTrack({"alice", TrackRole::Participant, "Alice"}, 0);
  for (int i = 0; i < 2; ++i)
    push(*engine, "alice", concat({speech(300), silence(300)}));
  engine->stop(2s);
  Require(engine->stats().decode_failures == 1, "a thrown non-std exception is a decode failure");
  Require(c.size() == 1, "the following phrase still decodes");
}

}  // namespace

int main() {
  TestEmitsPhraseWithRoleNameAndTimes();
  TestSeparatesTracksAndKeepsDecodeOrder();
  TestRemoveTrackFlushesOpenPhrase();
  TestResetSeparatesSpeechAndPreservesRemoteTrack();
  TestResetAcceptsReplacementSampleRateAndKeepsIdentity();
  TestIgnoresUnknownAndRemovedTracks();
  TestStopDrainsQueuedPhrases();
  TestStopWithZeroTimeoutCountsDroppedPhrases();
  TestDecodeFailureIsCountedAndSessionContinues();
  TestEmptyTextIsNotEmitted();
  TestReportsDelayWhenQueueFallsBehindAndRecovers();
  TestTenTracksTwoSpeakingAllPhrasesArriveAndQueueDrains();
  TestPushAfterStopIsIgnored();
  TestThrowingPhraseCallbackKeepsEngineRunningAndIsNotADecodeFailure();
  TestDelayedFlagClearsWhenQueueDrainsWithoutNewPhrase();
  TestThrowingDelayedCallbackDoesNotCrashAndPhrasesStillArrive();
  TestFailingTrackIsIsolatedAndCounted();
  TestNonStdExceptionFromRecogniserIsCountedAsFailure();
  return EXIT_SUCCESS;
}
