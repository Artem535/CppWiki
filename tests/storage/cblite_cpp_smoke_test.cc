#include <cbl++/CouchbaseLite.hh>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kDatabaseName = "cppwiki-cblite-smoke";

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
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
    std::cerr << "FAIL: Couchbase Lite error domain=" << error.domain << " code=" << error.code
              << '\n';
    std::exit(EXIT_FAILURE);
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    std::exit(EXIT_FAILURE);
  }

  std::filesystem::remove_all(test_directory);
}

}  // namespace

auto main() -> int {
  SmokeOpenCloseDeleteDatabase();

  std::cout << "cppwiki_cblite_cpp_smoke_tests passed\n";
  return EXIT_SUCCESS;
}
