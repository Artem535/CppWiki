#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "transcription/resampler.h"

namespace {

using cppwiki::transcription::Resampler;

void Require(bool condition, std::string_view message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", std::string(message).c_str());
    std::exit(EXIT_FAILURE);
  }
}

void RequireNear(double value, double expected, double tolerance, std::string_view message) {
  Require(std::abs(value - expected) <= tolerance, message);
}

std::vector<int16_t> sine(double hz, int rate, int samples, double amplitude) {
  std::vector<int16_t> out(samples);
  for (int i = 0; i < samples; ++i) {
    out[i] = static_cast<int16_t>(amplitude * std::sin(2.0 * std::numbers::pi * hz * i / rate));
  }
  return out;
}

double rms(const std::vector<float>& v, size_t from) {
  double sum = 0;
  for (size_t i = from; i < v.size(); ++i)
    sum += static_cast<double>(v[i]) * v[i];
  return std::sqrt(sum / static_cast<double>(v.size() - from));
}

void TestOutputLengthIsInputOverRatio() {
  Resampler r(48000);
  Require(r.process(std::vector<int16_t>(480, 0)).size() == 160,
          "480 samples at 48 kHz must yield 160 samples");
  Require(r.process(std::vector<int16_t>(4800, 0)).size() == 1600,
          "4800 samples at 48 kHz must yield 1600 samples");
}

void TestPreservesConstantSignal() {
  Resampler r(48000);
  const auto out = r.process(std::vector<int16_t>(4800, 10000));
  RequireNear(static_cast<double>(out.back()), 10000.0 / 32768.0, 1e-3,
              "a constant signal must keep its level");
}

void TestPassesOneKilohertzAndRejectsTwelveKilohertz() {
  Resampler low(48000);
  const auto kept = low.process(sine(1000, 48000, 9600, 10000));
  RequireNear(rms(kept, 800), 10000.0 / 32768.0 / std::sqrt(2.0), 0.015,
              "1 kHz must pass at its RMS level");

  Resampler high(48000);
  const auto cut = high.process(sine(12000, 48000, 9600, 10000));
  Require(rms(cut, 800) < 0.01 * 10000.0 / 32768.0, "12 kHz must be attenuated below 1%");
}

void TestChunkingDoesNotChangeOutput() {
  std::vector<int16_t> input = sine(440, 48000, 4801, 8000);
  Resampler whole(48000);
  const auto expected = whole.process(input);

  Resampler pieces(48000);
  std::vector<float> got;
  for (size_t i = 0; i < input.size(); i += 7) {
    const size_t n = std::min<size_t>(7, input.size() - i);
    const auto part = pieces.process(std::span<const int16_t>(input.data() + i, n));
    got.insert(got.end(), part.begin(), part.end());
  }
  Require(got.size() == expected.size(), "chunked output length must match whole-stream output");
  for (size_t i = 0; i < got.size(); ++i) {
    RequireNear(static_cast<double>(got[i]), static_cast<double>(expected[i]), 1e-6,
                "chunked output samples must match whole-stream output");
  }
}

void TestSixteenKilohertzIsPassThrough() {
  Resampler r(16000);
  const auto out = r.process(std::vector<int16_t>{0, 16384, -16384});
  Require(out.size() == 3, "16 kHz input must not change the sample count");
  RequireNear(static_cast<double>(out[1]), 0.5, 1e-6, "16384/32768 must map to 0.5");
  RequireNear(static_cast<double>(out[2]), -0.5, 1e-6, "-16384/32768 must map to -0.5");
}

void TestRejectsRatesThatAreNotMultiplesOfSixteenKilohertz() {
  bool rejected = false;
  try {
    Resampler r(44100);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  Require(rejected, "44100 Hz must be rejected");
  rejected = false;
  try {
    Resampler r(0);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  Require(rejected, "0 Hz must be rejected");
}

}  // namespace

int main() {
  TestOutputLengthIsInputOverRatio();
  TestPreservesConstantSignal();
  TestPassesOneKilohertzAndRejectsTwelveKilohertz();
  TestChunkingDoesNotChangeOutput();
  TestSixteenKilohertzIsPassThrough();
  TestRejectsRatesThatAreNotMultiplesOfSixteenKilohertz();
  return EXIT_SUCCESS;
}
