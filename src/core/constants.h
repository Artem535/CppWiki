#ifndef CPPWIKI_SRC_CORE_CONSTANTS_H_
#define CPPWIKI_SRC_CORE_CONSTANTS_H_

#include <cstdint>
#include <string_view>

namespace cppwiki::constants {

inline constexpr std::string_view kApplicationName = "CppWiki";
inline constexpr std::string_view kApplicationVersion = "0.1.0";
inline constexpr std::string_view kOrganizationName = "CppWiki";

inline constexpr int kInitialWindowWidth = 1280;
inline constexpr int kInitialWindowHeight = 800;
inline constexpr int kPageListInitialWidth = 280;
inline constexpr int kPageListMinimumWidth = 220;
inline constexpr int kPageListMaximumWidth = 360;
inline constexpr int kDefaultApplicationFontPointSize = 11;

inline constexpr int kBridgeApiVersion = 1;
inline constexpr std::string_view kDocumentsBridgeObjectName = "wikiDocuments";
inline constexpr std::string_view kDocumentsBridgeNamespace = "wiki.documents";
inline constexpr std::string_view kBridgeMethodGetBridgeInfo = "GetBridgeInfo";
inline constexpr std::string_view kBridgeMethodGetInitialDocument = "GetInitialDocument";
inline constexpr std::string_view kBridgeMethodListDocuments = "ListDocuments";
inline constexpr std::string_view kBridgeMethodCreateDocument = "CreateDocument";
inline constexpr std::string_view kBridgeMethodCreateChildDocument = "CreateChildDocument";
inline constexpr std::string_view kBridgeMethodRenameDocument = "RenameDocument";
inline constexpr std::string_view kBridgeMethodLoadDocument = "LoadDocument";
inline constexpr std::string_view kBridgeMethodOpenDocument = "OpenDocument";
inline constexpr std::string_view kBridgeMethodUpdateSnapshot = "UpdateSnapshot";

inline constexpr std::string_view kDatabaseDirectoryName = "database";
inline constexpr std::string_view kDefaultPageTitle = "Welcome to CppWiki";
inline constexpr std::string_view kDefaultPageBodyText =
    "This is your first local page. Select it from the list and start writing.";
inline constexpr std::string_view kNewDocumentTitle = "Untitled note";
inline constexpr std::string_view kNewDocumentActionTitle = "New note";
inline constexpr std::string_view kAddChildActionId = "cppwiki-add-child-action";
inline constexpr std::string_view kNewDocumentActionId = "cppwiki-new-document-action";
inline constexpr std::string_view kQlementineDarkThemePath =
    "third_party/qlementine/showcase/resources/themes/dark.json";
inline constexpr std::string_view kApplicationQssPath = "src/app/cppwiki.qss";
inline constexpr std::string_view kEditorFallbackHtmlPath = "src/app/editor_fallback.html";
inline constexpr int kServerApiVersion = 1;
inline constexpr std::string_view kServerServiceName = "cppwiki-server";
inline constexpr std::string_view kDefaultServerBindHost = "127.0.0.1";
inline constexpr std::uint16_t kDefaultServerPort = 8080;
inline constexpr bool kDefaultServerSwaggerEnabled = true;
inline constexpr std::string_view kDefaultServerLogLevel = "info";
inline constexpr std::string_view kDefaultServerConfigPath = "config/server.yaml";
inline constexpr std::string_view kDefaultSwaggerTitle = "CppWiki Server API";
inline constexpr std::string_view kDefaultSwaggerDescription =
    "CppWiki local/backend service API.";

inline constexpr std::string_view kSettingsAppDataDirectoryKey = "paths/appDataDirectory";
inline constexpr std::string_view kSettingsDatabaseDirectoryKey = "paths/databaseDirectory";
inline constexpr std::string_view kSettingsEditorDistDirectoryKey = "paths/editorDistDirectory";
inline constexpr std::string_view kSettingsApplicationFontPointSizeKey =
    "appearance/applicationFontPointSize";

inline constexpr std::string_view kDocumentsCollectionName = "documents";
inline constexpr std::string_view kDocumentsIndexDocumentId = "cppwiki-document-index";
}  // namespace cppwiki::constants

#endif  // CPPWIKI_SRC_CORE_CONSTANTS_H_
