#include <cbl++/CouchbaseLite.hh>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kDatabaseName = "cppwiki-cblite-smoke";

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto Slice(std::string_view value) -> cbl::slice {
  return cbl::slice(value.data(), value.size());
}

auto SmokeOpenCloseDeleteDatabase() -> void {
  const auto test_directory = std::filesystem::temp_directory_path() / "cppwiki-cblite-smoke";
  std::filesystem::remove_all(test_directory);
  std::filesystem::create_directories(test_directory);

  try {
    auto config = CBLDatabaseConfiguration_Default();
    const auto directory = test_directory.string();
    config.directory = Slice(directory);

    {
      cbl::Database database(Slice(kDatabaseName), config);
      Require(static_cast<bool>(database), "database should open");
      database.close();
    }

    cbl::Database::deleteDatabase(Slice(kDatabaseName), Slice(directory));
  } catch (const CBLError& error) {
    spdlog::error("FAIL: Couchbase Lite error domain={} code={}",
                  static_cast<int>(error.domain),
                  error.code);
    std::exit(EXIT_FAILURE);
  } catch (const std::exception& error) {
    spdlog::error("FAIL: {}", error.what());
    std::exit(EXIT_FAILURE);
  }

  std::filesystem::remove_all(test_directory);
}

}  // namespace

auto main() -> int {
  SmokeOpenCloseDeleteDatabase();

  spdlog::info("cppwiki_cblite_cpp_smoke_tests passed");
  return EXIT_SUCCESS;
}
