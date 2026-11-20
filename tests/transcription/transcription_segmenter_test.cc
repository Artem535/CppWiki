#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "tests/transcription/transcription_test_support.h"
#include "transcription/phrase_segmenter.h"

namespace {

using cppwiki::transcription::PhraseSegmenter;
using cppwiki::transcription::SpeechSegment;
using cppwiki::transcription::testing::FakeVad;

void Require(bool condition, std::string_view message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", std::string(message).c_str());
    std::exit(EXIT_FAILURE);
  }
}

std::vector<float> block(float value, size_t n) {
  return std::vector<float>(n, value);
}

PhraseSegmenter makeSegmenter() {
  return PhraseSegmenter(std::make_unique<FakeVad>());
}

void TestEmitsClosedPhraseWithStartSample() {
  auto seg = makeSegmenter();
  std::vector<float> audio = block(0.0F, 1024);
  const auto speechBlock = block(0.5F, 1024);
  audio.insert(audio.end(), speechBlock.begin(), speechBlock.end());
  const auto quiet = block(0.0F, 512);
  audio.insert(audio.end(), quiet.begin(), quiet.end());

  const auto phrases = seg.feed(audio);
  Require(phrases.size() == 1, "one closed phrase must be emitted");
  Require(phrases[0].start_sample == 1024, "the phrase must start at the first speech window");
  Require(phrases[0].samples.size() == 1024, "the phrase must contain exactly the speech windows");
}

void TestBuffersPartialWindowsAcrossCalls() {
  auto seg = makeSegmenter();
  std::vector<float> all = block(0.5F, 1024);
  const auto quiet = block(0.0F, 512);
  all.insert(all.end(), quiet.begin(), quiet.end());

  std::vector<SpeechSegment> got;
  for (size_t i = 0; i < all.size(); i += 100) {
    const size_t n = std::min<size_t>(100, all.size() - i);
    auto part = seg.feed(std::span<const float>(all.data() + i, n));
    got.insert(got.end(), std::make_move_iterator(part.begin()),
               std::make_move_iterator(part.end()));
  }
  Require(got.size() == 1, "partial windows buffered across calls must still close one phrase");
  Require(got[0].start_sample == 0, "the phrase must start at stream position 0");
}

void TestFlushClosesOpenPhraseAndPadsPartialWindow() {
  auto seg = makeSegmenter();
  Require(seg.feed(block(0.5F, 700)).empty(), "a partial window emits nothing yet");
  const auto phrases = seg.flush();
  Require(phrases.size() == 1, "flush must close the open phrase");
  Require(phrases[0].samples.size() == 1024, "flush must zero-pad the partial window to 1024");
}

void TestFlushWithNothingOpenReturnsNothing() {
  auto seg = makeSegmenter();
  Require(seg.feed(block(0.0F, 2048)).empty(), "silence emits nothing");
  Require(seg.flush().empty(), "flush with nothing open must return nothing");
}

}  // namespace

int main() {
  TestEmitsClosedPhraseWithStartSample();
  TestBuffersPartialWindowsAcrossCalls();
  TestFlushClosesOpenPhraseAndPadsPartialWindow();
  TestFlushWithNothingOpenReturnsNothing();
  return EXIT_SUCCESS;
}
