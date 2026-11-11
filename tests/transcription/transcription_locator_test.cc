#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "transcription/model_locator.h"

namespace fs = std::filesystem;
using cppwiki::transcription::locateModels;
using cppwiki::transcription::modelRootCandidates;
using cppwiki::transcription::Platform;

namespace {

void Require(bool condition, std::string_view message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", std::string(message).c_str());
    std::exit(EXIT_FAILURE);
  }
}

int currentPid() {
#ifdef _WIN32
  return _getpid();
#else
  return static_cast<int>(::getpid());
#endif
}

struct TempDir {
  TempDir() {
    path = fs::temp_directory_path() /
           ("cppwiki-locator-" + std::to_string(currentPid()) + "-" + std::to_string(counter++));
    fs::create_directories(path / "bin");  // app dir must exist for "bin/.." to resolve
  }
  ~TempDir() {
    fs::remove_all(path);
  }
  static inline int counter = 0;
  fs::path path;
};

void touch(const fs::path& p) {
  fs::create_directories(p.parent_path());
  std::ofstream(p) << "x";
}

void writeModelTree(const fs::path& root) {
  for (const char* f : {"encoder.int8.onnx", "decoder.onnx", "joiner.onnx", "tokens.txt"}) {
    touch(root / "gigaam-v3-rnnt" / f);
  }
  touch(root / "silero_vad.onnx");
}

void TestCandidatesPerPlatform() {
  const fs::path app = "/opt/app/bin";
  Require(modelRootCandidates(app, Platform::Linux, {}).front() ==
              fs::path("/opt/app/bin/../share/cppwiki/models"),
          "Linux must install models under share/cppwiki/models");
  Require(modelRootCandidates("/A/CppWiki.app/Contents/MacOS", Platform::MacOS, {}).front() ==
              fs::path("/A/CppWiki.app/Contents/MacOS/../Resources/models"),
          "macOS must install models under Resources/models");
  Require(modelRootCandidates("C:/CppWiki", Platform::Windows, {}).front() ==
              fs::path("C:/CppWiki/models"),
          "Windows must install models beside the executable");
}

void TestOverrideComesFirst() {
  const auto c = modelRootCandidates("/opt/app/bin", Platform::Linux, "/custom");
  Require(c.size() == 3, "env override adds exactly one candidate");
  Require(c.front() == fs::path("/custom"), "the env override must come first");
}

void TestFindsCompleteTree() {
  TempDir dir;
  writeModelTree(dir.path / "share" / "cppwiki" / "models");
  const auto result = locateModels(dir.path / "bin", Platform::Linux);
  Require(result.paths.has_value(), "a complete installed tree must be found: " + result.error);
  Require(result.paths->encoder.filename() == "encoder.int8.onnx", "encoder file name");
  Require(result.paths->vad.filename() == "silero_vad.onnx", "vad file name");
  Require(result.error.empty(), "no error on success");
}

void TestReportsMissingFileByName() {
  TempDir dir;
  const auto root = dir.path / "share" / "cppwiki" / "models";
  writeModelTree(root);
  fs::remove(root / "gigaam-v3-rnnt" / "joiner.onnx");
  const auto result = locateModels(dir.path / "bin", Platform::Linux);
  Require(!result.paths.has_value(), "an incomplete tree must not resolve");
  Require(result.error.find("joiner.onnx") != std::string::npos,
          "the error must name the missing file");
}

void TestReportsWhenNoCandidateExists() {
  TempDir dir;
  const auto result = locateModels(dir.path / "bin", Platform::Linux);
  Require(!result.paths.has_value(), "no candidates must not resolve");
  Require(!result.error.empty(), "no candidates must produce an error");
}

void TestFindsModelsDownloadedIntoTheBuildTree() {
  TempDir dir;
  writeModelTree(dir.path / "build" / "transcription-models");
  const auto result = locateModels(dir.path / "build", Platform::Linux);
  Require(result.paths.has_value(), "the build-tree fallback must be found: " + result.error);
  Require(result.paths->vad.parent_path() == dir.path / "build" / "transcription-models",
          "the build-tree fallback root must be transcription-models");
}

void TestIncompleteInstalledTreeIsReportedOverAMissingBuildTree() {
  TempDir dir;
  const auto root = dir.path / "share" / "cppwiki" / "models";
  writeModelTree(root);
  fs::remove(root / "silero_vad.onnx");
  const auto result = locateModels(dir.path / "bin", Platform::Linux);
  Require(result.error.find("silero_vad.onnx") != std::string::npos,
          "a partly installed tree must win over the absent build-tree fallback");
}

void TestOverrideWinsOverPlatformDefault() {
  TempDir dir;
  writeModelTree(dir.path / "override");
  const auto result =
      locateModels(dir.path / "bin", Platform::Linux, (dir.path / "override").string());
  Require(result.paths.has_value(), "the override root must be used: " + result.error);
  Require(result.paths->tokens.parent_path().parent_path() == dir.path / "override",
          "the resolved tree must live under the override root");
}

}  // namespace

int main() {
  TestCandidatesPerPlatform();
  TestOverrideComesFirst();
  TestFindsCompleteTree();
  TestReportsMissingFileByName();
  TestReportsWhenNoCandidateExists();
  TestFindsModelsDownloadedIntoTheBuildTree();
  TestIncompleteInstalledTreeIsReportedOverAMissingBuildTree();
  TestOverrideWinsOverPlatformDefault();
  return EXIT_SUCCESS;
}
