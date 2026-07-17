#ifndef CPPWIKI_SRC_APP_PROGRAM_SETTINGS_H_
#define CPPWIKI_SRC_APP_PROGRAM_SETTINGS_H_

#include <QString>

class QSettings;

namespace cppwiki {

class ProgramSettings final {
 public:
  ProgramSettings(QString application_name, QString application_version, QString organization_name,
                  QString app_data_directory, QString database_directory,
                  QString editor_dist_directory, QString backend_base_url, bool backend_enabled,
                  QString auth_authorization_url, QString auth_token_url, QString auth_client_id,
                  QString auth_redirect_uri, bool auth_enabled, bool demo_collaboration_enabled,
                  QString demo_collaboration_user_id, bool sync_enabled,
                  int application_font_point_size, bool ai_features_enabled,
                  bool ai_autocomplete_enabled, bool ai_inline_suggestions_enabled);

  [[nodiscard]] static auto FromDefaults() -> ProgramSettings;
  [[nodiscard]] static auto FromSettings(const QSettings& settings) -> ProgramSettings;
  void SaveToSettings(QSettings& settings) const;

  [[nodiscard]] auto ApplicationName() const -> const QString&;
  [[nodiscard]] auto ApplicationVersion() const -> const QString&;
  [[nodiscard]] auto OrganizationName() const -> const QString&;
  [[nodiscard]] auto AppDataDirectory() const -> const QString&;
  [[nodiscard]] auto DatabaseDirectory() const -> const QString&;
  [[nodiscard]] auto EditorDistDirectory() const -> const QString&;
  [[nodiscard]] auto BackendBaseUrl() const -> const QString&;
  [[nodiscard]] auto BackendEnabled() const -> bool;
  [[nodiscard]] auto AuthAuthorizationUrl() const -> const QString&;
  [[nodiscard]] auto AuthTokenUrl() const -> const QString&;
  [[nodiscard]] auto AuthClientId() const -> const QString&;
  [[nodiscard]] auto AuthRedirectUri() const -> const QString&;
  [[nodiscard]] auto AuthEnabled() const -> bool;
  [[nodiscard]] auto DemoCollaborationEnabled() const -> bool;
  [[nodiscard]] auto DemoCollaborationUserId() const -> const QString&;
  [[nodiscard]] auto SyncEnabled() const -> bool;
  [[nodiscard]] auto ApplicationFontPointSize() const -> int;
  [[nodiscard]] auto AiFeaturesEnabled() const -> bool;
  [[nodiscard]] auto AiAutocompleteEnabled() const -> bool;
  // Separate opt-in for inline ghost-text suggestions (issue #59); independent
  // of AiFeaturesEnabled()/AiAutocompleteEnabled().
  [[nodiscard]] auto AiInlineSuggestionsEnabled() const -> bool;

 private:
  QString application_name_;
  QString application_version_;
  QString organization_name_;
  QString app_data_directory_;
  QString database_directory_;
  QString editor_dist_directory_;
  QString backend_base_url_;
  bool backend_enabled_;
  QString auth_authorization_url_;
  QString auth_token_url_;
  QString auth_client_id_;
  QString auth_redirect_uri_;
  bool auth_enabled_;
  bool demo_collaboration_enabled_;
  QString demo_collaboration_user_id_;
  bool sync_enabled_;
  int application_font_point_size_;
  bool ai_features_enabled_;
  bool ai_autocomplete_enabled_;
  bool ai_inline_suggestions_enabled_;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_PROGRAM_SETTINGS_H_
