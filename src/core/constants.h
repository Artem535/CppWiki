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
inline constexpr int kEditModeInactivityTimeoutMinutes = 3;

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
// File import/export (issue #82): native, QFileDialog-backed save/open, used for
// exporting/importing a document kind's raw content (e.g. nbformat JSON, Excalidraw scene
// JSON) as a standalone file. Kind-agnostic on purpose — the JS side owns interpreting/
// producing the file content; this pair only moves bytes to/from disk through a native dialog
// instead of exposing Excalidraw's/Chromium's own (unsupported in this embedding, see
// page.cc's fileSystemAccessRequested handling) file pickers.
inline constexpr std::string_view kBridgeMethodExportTextToFile = "ExportTextToFile";
inline constexpr std::string_view kBridgeMethodImportTextFromFile = "ImportTextFromFile";

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
inline constexpr std::string_view kDefaultSwaggerDescription = "CppWiki local/backend service API.";

inline constexpr std::string_view kSettingsAppDataDirectoryKey = "paths/appDataDirectory";
inline constexpr std::string_view kSettingsDatabaseDirectoryKey = "paths/databaseDirectory";
inline constexpr std::string_view kSettingsEditorDistDirectoryKey = "paths/editorDistDirectory";
inline constexpr std::string_view kSettingsBackendEnabledKey = "backend/enabled";
inline constexpr std::string_view kSettingsBackendBaseUrlKey = "backend/baseUrl";
inline constexpr std::string_view kSettingsAuthEnabledKey = "auth/enabled";
inline constexpr std::string_view kSettingsAuthAuthorizationUrlKey = "auth/authorizationUrl";
inline constexpr std::string_view kSettingsAuthTokenUrlKey = "auth/tokenUrl";
inline constexpr std::string_view kSettingsAuthClientIdKey = "auth/clientId";
inline constexpr std::string_view kSettingsAuthRedirectUriKey = "auth/redirectUri";
inline constexpr std::string_view kSettingsDemoCollaborationEnabledKey =
    "demo/collaborationEnabled";
inline constexpr std::string_view kSettingsDemoCollaborationUserIdKey = "demo/collaborationUserId";
inline constexpr std::string_view kSettingsSyncEnabledKey = "sync/enabled";
inline constexpr std::string_view kSettingsApplicationFontPointSizeKey =
    "appearance/applicationFontPointSize";
inline constexpr std::string_view kSettingsAiFeaturesEnabledKey = "ai/featuresEnabled";
inline constexpr std::string_view kSettingsAiAutocompleteEnabledKey = "ai/autocompleteEnabled";
// Separate opt-in for continuous inline ghost-text suggestions (issue #59):
// deliberately independent of kSettingsAiFeaturesEnabledKey so users can keep
// the toolbar/slash-menu AI features on while disabling inline suggestions
// (or vice versa).
inline constexpr std::string_view kSettingsAiInlineSuggestionsEnabledKey =
    "ai/inlineSuggestionsEnabled";
inline constexpr std::string_view kSettingsAccentColorKey = "appearance/accentColor";
inline constexpr std::string_view kDefaultAccentColorKey = "blue";

inline constexpr std::string_view kDocumentsCollectionName = "documents";
inline constexpr std::string_view kLocalDocumentsCollectionName = "local_documents";
inline constexpr std::string_view kDocumentsIndexDocumentId = "cppwiki-document-index";
inline constexpr std::string_view kLocalDocumentsIndexDocumentId = "cppwiki-local-document-index";
inline constexpr std::string_view kConflictsIndexDocumentId = "cppwiki-conflict-index";
inline constexpr std::string_view kConflictDocumentIdPrefix = "cppwiki-conflict-";
inline constexpr std::string_view kWorkspaceDocumentIdPrefix = "workspace:";
inline constexpr std::string_view kWorkspaceRootDocumentType = "workspace";
inline constexpr std::string_view kDefaultAuthRedirectUri = "http://127.0.0.1:38080/auth/callback";
inline constexpr std::string_view kDefaultAuthScopes =
    "openid profile email groups roles offline_access";
inline constexpr std::string_view kAuthTokenStoreEntryKey = "desktop-auth-session";

// AI provider request settings (ADR-010/ADR-012). Kept as plain constants (not
// config/server.yaml) since these describe *how the model is instructed to behave*
// rather than deployment-specific config (endpoint/key/timeout, which do live in
// config/server.yaml) — edit these directly to change the AI features' behavior.
inline constexpr std::string_view kAiDefaultModel = "gpt-4o-mini";

// System prompt for the "rewrite"/"improve selection" AI toolbar action (default mode
// when AiChatRequestDto::mode is unset).
inline constexpr std::string_view kAiSystemPromptRewrite =
    "Rewrite/improve the following selection as instructed.";

// System prompt for the "autocomplete"/"continue writing" AI slash-menu action.
inline constexpr std::string_view kAiSystemPromptAutocomplete =
    "Continue writing the following document naturally.";

// System prompt for "inline" mode: continuous ghost-text completions fired on every
// typing pause (issue #59). Kept deliberately short and steered toward a brief,
// single continuation rather than a full rewrite or an open-ended "keep writing"
// response — this keeps latency and output size down for the self-hosted provider.
inline constexpr std::string_view kAiSystemPromptInline =
    "Continue the user's text with a short, natural completion (a few words to one "
    "sentence). Output only the continuation text, with no explanation, no quotes, "
    "and no repetition of the given text.";

}  // namespace cppwiki::constants

#endif  // CPPWIKI_SRC_CORE_CONSTANTS_H_
