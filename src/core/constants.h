#ifndef CPPWIKI_SRC_CORE_CONSTANTS_H_
#define CPPWIKI_SRC_CORE_CONSTANTS_H_

#include <string_view>

namespace cppwiki::constants {

inline constexpr std::string_view kApplicationName = "CppWiki";
inline constexpr std::string_view kApplicationVersion = "0.1.0";
inline constexpr std::string_view kOrganizationName = "CppWiki";

inline constexpr int kBridgeApiVersion = 1;
inline constexpr std::string_view kDocumentsBridgeObjectName = "wikiDocuments";
inline constexpr std::string_view kDocumentsBridgeNamespace = "wiki.documents";
inline constexpr std::string_view kBridgeMethodGetBridgeInfo = "getBridgeInfo";
inline constexpr std::string_view kBridgeMethodGetInitialDocument = "getInitialDocument";
inline constexpr std::string_view kBridgeMethodUpdateSnapshot = "updateSnapshot";

inline constexpr std::string_view kDatabaseDirectoryName = "database";
inline constexpr std::string_view kDefaultPageTitle = "Getting Started";

inline constexpr std::string_view kSettingsAppDataDirectoryKey = "paths/appDataDirectory";
inline constexpr std::string_view kSettingsDatabaseDirectoryKey = "paths/databaseDirectory";
inline constexpr std::string_view kSettingsEditorDistDirectoryKey = "paths/editorDistDirectory";

}  // namespace cppwiki::constants

#endif  // CPPWIKI_SRC_CORE_CONSTANTS_H_
