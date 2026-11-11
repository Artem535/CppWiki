#ifndef CPPWIKI_SRC_TRANSCRIPTION_MODEL_LOCATOR_H_
#define CPPWIKI_SRC_TRANSCRIPTION_MODEL_LOCATOR_H_

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppwiki::transcription {

struct ModelPaths {
  std::filesystem::path encoder;
  std::filesystem::path decoder;
  std::filesystem::path joiner;
  std::filesystem::path tokens;
  std::filesystem::path vad;
};

enum class Platform { Linux, MacOS, Windows };

Platform currentPlatform();

// Directories that may hold <root>/gigaam-v3-rnnt/* and <root>/silero_vad.onnx,
// in lookup order: CPPWIKI_MODELS_DIR (`env_override`, may be empty), the
// installed location, then <app_dir>/transcription-models of a build tree.
std::vector<std::filesystem::path> modelRootCandidates(const std::filesystem::path& app_dir,
                                                       Platform platform,
                                                       std::string_view env_override);

struct LocateResult {
  std::optional<ModelPaths> paths;
  std::string error;  // user-readable, empty on success
};

LocateResult locateModels(const std::filesystem::path& app_dir,
                          Platform platform = currentPlatform(),
                          std::string_view env_override = {});

}  // namespace cppwiki::transcription

#endif  // CPPWIKI_SRC_TRANSCRIPTION_MODEL_LOCATOR_H_
