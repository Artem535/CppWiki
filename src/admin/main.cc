#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "admin/admin_client.h"
#include "admin/admin_token_store.h"

namespace {

namespace Container = ftxui::Container;

using ftxui::Color;
using ftxui::Component;
using ftxui::Element;
using ftxui::Renderer;
using ftxui::ScreenInteractive;
using ftxui::separator;
using ftxui::text;
using ftxui::vbox;
using ftxui::window;

constexpr const char* kDefaultBaseUrl = "http://127.0.0.1:8080";

struct AppState final {
  std::string base_url = kDefaultBaseUrl;
  std::string access_token;
  std::vector<std::string> workspaces;
  std::string status_line = "Not signed in. Paste an admin access token to begin.";
  bool status_is_error = false;
};

void SetStatus(AppState& state, std::string message, bool is_error) {
  state.status_line = std::move(message);
  state.status_is_error = is_error;
}

}  // namespace

int main(int argc, char** argv) {
  cppwiki::admin::AdminTokenStore token_store(cppwiki::admin::AdminTokenStore::DefaultTokenFilePath());

  AppState state;
  if (const char* env_base_url = std::getenv("CPPWIKI_ADMIN_BASE_URL");
      env_base_url != nullptr && *env_base_url != '\0') {
    state.base_url = env_base_url;
  }
  if (const auto loaded_token = token_store.Load(); loaded_token) {
    state.access_token = *loaded_token;
    SetStatus(state, "Loaded a stored admin session token from " + token_store.DefaultTokenFilePath(),
              false);
  }

  // --help / --version support without starting the interactive TUI, so the
  // binary is scriptable/verifiable in CI without a real terminal.
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      std::cout << "cppwiki-admin - terminal admin console for cppwiki_server\n\n"
                << "Usage: cppwiki-admin [options]\n\n"
                << "Options:\n"
                << "  -h, --help       Show this help and exit\n"
                << "      --version    Show version and exit\n\n"
                << "Environment:\n"
                << "  CPPWIKI_ADMIN_BASE_URL   Base URL of cppwiki_server "
                << "(default: " << kDefaultBaseUrl << ")\n\n"
                << "The admin session token is stored at:\n  "
                << cppwiki::admin::AdminTokenStore::DefaultTokenFilePath() << "\n";
      return 0;
    }
    if (arg == "--version") {
      std::cout << "cppwiki-admin 0.1.0 (stage 1: workspace listing/creation, sync overview)\n";
      return 0;
    }
  }

  cppwiki::admin::AdminClient client(state.base_url, state.access_token);

  auto screen = ScreenInteractive::TerminalOutput();

  std::string token_input;
  std::string workspace_input;
  std::string sync_overview_text = "Not fetched yet.";

  auto refresh_workspaces = [&] {
    client.SetAccessToken(state.access_token);
    client.SetBaseUrl(state.base_url);
    const auto outcome = client.ListWorkspaces();
    if (!outcome.success) {
      SetStatus(state, "List workspaces failed: " + outcome.error_message, true);
      return;
    }
    state.workspaces = outcome.value;
    SetStatus(state, "Loaded " + std::to_string(state.workspaces.size()) + " workspace(s).", false);
  };

  auto login_input = ftxui::Input(&token_input, "paste admin access token");
  auto login_button = ftxui::Button("Sign in", [&] {
    if (token_input.empty()) {
      SetStatus(state, "Token field is empty.", true);
      return;
    }
    state.access_token = token_input;
    client.SetAccessToken(state.access_token);
    if (!token_store.Save(state.access_token)) {
      SetStatus(state, "Signed in for this session, but failed to persist the token to "
                            + token_store.DefaultTokenFilePath(),
                true);
    } else {
      SetStatus(state, "Signed in and saved admin session token.", false);
    }
    token_input.clear();
    refresh_workspaces();
  });
  auto logout_button = ftxui::Button("Sign out", [&] {
    state.access_token.clear();
    state.workspaces.clear();
    client.SetAccessToken("");
    if (!token_store.Clear()) {
      SetStatus(state, "Signed out, but failed to clear the stored token file.", true);
      return;
    }
    SetStatus(state, "Signed out and cleared the stored token.", false);
  });

  auto refresh_button = ftxui::Button("Refresh workspaces", [&] { refresh_workspaces(); });

  auto workspace_input_component = ftxui::Input(&workspace_input, "new-workspace-id");
  auto create_button = ftxui::Button("Create workspace", [&] {
    if (workspace_input.empty()) {
      SetStatus(state, "Workspace id field is empty.", true);
      return;
    }
    client.SetAccessToken(state.access_token);
    client.SetBaseUrl(state.base_url);
    const auto outcome = client.CreateWorkspace(workspace_input);
    if (!outcome.success) {
      SetStatus(state, "Create workspace failed: " + outcome.error_message, true);
      return;
    }
    SetStatus(state, "Created workspace '" + outcome.value.id + "'.", false);
    workspace_input.clear();
    refresh_workspaces();
  });

  auto sync_button = ftxui::Button("Fetch sync overview", [&] {
    client.SetAccessToken(state.access_token);
    client.SetBaseUrl(state.base_url);
    const auto outcome = client.FetchSyncOverview();
    if (!outcome.success) {
      SetStatus(state, "Sync overview failed: " + outcome.error_message, true);
      sync_overview_text = "Not available.";
      return;
    }
    const auto& overview = outcome.value;
    sync_overview_text = "available=" + std::string(overview.available ? "true" : "false") +
                         " enabled=" + std::string(overview.enabled ? "true" : "false") + "\n" +
                         "gateway=" + overview.gateway_url + "\n" +
                         "database=" + overview.database_name + "\n" +
                         "status=" + overview.status_text;
    SetStatus(state, "Fetched sync overview.", false);
  });

  auto quit_button = ftxui::Button("Quit", screen.ExitLoopClosure());

  auto layout = Container::Vertical({
      login_input,
      Container::Horizontal({login_button, logout_button}),
      refresh_button,
      workspace_input_component,
      create_button,
      sync_button,
      quit_button,
  });

  auto renderer = Renderer(layout, [&] {
    Element workspace_list = vbox({}) | ftxui::flex;
    {
      std::vector<Element> rows;
      for (const auto& workspace_id : state.workspaces) {
        rows.push_back(text(" - " + workspace_id));
      }
      if (rows.empty()) {
        rows.push_back(text(" (none loaded yet)") | ftxui::dim);
      }
      workspace_list = vbox(std::move(rows));
    }

    auto status = text(state.status_line);
    status = state.status_is_error ? status | ftxui::color(Color::Red)
                                    : status | ftxui::color(Color::Green);

    return window(
               text(" cppwiki-admin "),
               vbox({
                   text("Server: " + state.base_url),
                   text("Session: " + std::string(state.access_token.empty() ? "signed out"
                                                                             : "signed in")),
                   separator(),
                   text("Admin session token:"),
                   login_input->Render(),
                   ftxui::hbox({login_button->Render(), logout_button->Render()}),
                   separator(),
                   text("Workspaces:"),
                   workspace_list,
                   refresh_button->Render(),
                   separator(),
                   text("Create workspace:"),
                   workspace_input_component->Render(),
                   create_button->Render(),
                   separator(),
                   text("Sync overview:"),
                   text(sync_overview_text),
                   sync_button->Render(),
                   separator(),
                   status,
                   quit_button->Render(),
               })) |
           ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 60);
  });

  screen.Loop(renderer);
  return 0;
}
