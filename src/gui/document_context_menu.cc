#include "gui/document_context_menu.h"

#include <QIcon>
#include <QPushButton>
#include <QSize>
#include <QStyle>
#include <QVBoxLayout>
#include <utility>

namespace cppwiki::gui {

DocumentContextMenu::DocumentContextMenu(const Options& options, QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint) {
  setObjectName(QStringLiteral("documentContextMenu"));
  setAttribute(Qt::WA_DeleteOnClose);
  setFrameShape(QFrame::NoFrame);
  setStyleSheet(QStringLiteral(R"(
    QFrame#documentContextMenu {
      background-color: palette(base);
      border: 1px solid palette(mid);
      border-radius: 6px;
    }
    QFrame#documentContextMenu QPushButton {
      border: none;
      border-radius: 4px;
      min-height: 30px;
      padding: 4px 10px;
      text-align: left;
      background: transparent;
      color: palette(text);
    }
    QFrame#documentContextMenu QPushButton:hover {
      background-color: palette(highlight);
      color: palette(highlighted-text);
    }
    QFrame#documentContextMenu QPushButton:disabled {
      color: palette(mid);
    }
  )"));

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(2);

  AddNewDocumentOptions();
  AddButton(Action::kRenameTitle, QStringLiteral("Rename title"),
            QIcon::fromTheme(QStringLiteral("document-edit")), true);
  AddButton(Action::kMoveUp, QStringLiteral("Move up"), style()->standardIcon(QStyle::SP_ArrowUp),
            options.can_move_up);
  AddButton(Action::kMoveDown, QStringLiteral("Move down"),
            style()->standardIcon(QStyle::SP_ArrowDown), options.can_move_down);

  auto* separator = new QFrame(this);
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Plain);
  layout->addWidget(separator);

  AddButton(Action::kDeletePage, QStringLiteral("Delete page"),
            style()->standardIcon(QStyle::SP_TrashIcon), true);
}

void DocumentContextMenu::ShowAt(const QPoint& global_position) {
  adjustSize();
  move(global_position);
  show();
}

void DocumentContextMenu::AddButton(Action action, const QString& text, const QIcon& icon,
                                    bool enabled) {
  auto* button = new QPushButton(text, this);
  button->setIcon(icon);
  button->setIconSize(QSize(16, 16));
  button->setEnabled(enabled);
  button->setCursor(Qt::PointingHandCursor);
  button->setFocusPolicy(Qt::NoFocus);
  button->setFlat(true);

  if (auto* layout = qobject_cast<QVBoxLayout*>(this->layout())) {
    layout->addWidget(button);
  }

  connect(button, &QPushButton::clicked, this, [this, action]() {
    close();
    emit actionRequested(action);
  });
}

void DocumentContextMenu::AddNewDocumentOptions() {
  auto* toggle = new QPushButton(QStringLiteral("New document"), this);
  toggle->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  toggle->setIconSize(QSize(16, 16));
  toggle->setCursor(Qt::PointingHandCursor);
  toggle->setFocusPolicy(Qt::NoFocus);
  toggle->setFlat(true);

  if (auto* layout = qobject_cast<QVBoxLayout*>(this->layout())) {
    layout->addWidget(toggle);
  }

  // "Wiki page" is first/default and, when chosen, must produce byte-identical
  // behavior to the old single hardcoded "Add child page" action.
  AddKindOption(document::DocumentKind::kWikiPage, QStringLiteral("Wiki page"));
  AddKindOption(document::DocumentKind::kJupyterNotebook, QStringLiteral("Jupyter notebook"));
  AddKindOption(document::DocumentKind::kExcalidrawCanvas, QStringLiteral("Excalidraw canvas"));
  AddKindOption(document::DocumentKind::kOpenApiSpec, QStringLiteral("OpenAPI spec"));
  AddKindOption(document::DocumentKind::kProjectBoard, QStringLiteral("Project board"));

  for (auto* kind_button : std::as_const(new_document_kind_buttons_)) {
    kind_button->setVisible(false);
  }

  // The kind options are expanded/collapsed inline, in this same popup's
  // layout, rather than shown in a nested QMenu submenu. A nested QMenu
  // (itself Qt::Popup-flavored) stacked on top of this widget (also
  // Qt::Popup) never delivered clicks as QAction::triggered() in practice:
  // Qt's popup-stack close-on-outside-click handling tore down the whole
  // popup chain before the inner QMenu's own click routing ran, so choosing
  // a kind silently did nothing. Expanding inline reuses the exact
  // single-popup click pattern that already works for Rename/Move/Delete
  // above (AddButton()).
  connect(toggle, &QPushButton::clicked, this, [this]() {
    const bool now_visible =
        !new_document_kind_buttons_.isEmpty() && !new_document_kind_buttons_.first()->isVisible();
    for (auto* kind_button : std::as_const(new_document_kind_buttons_)) {
      kind_button->setVisible(now_visible);
    }
    adjustSize();
  });
}

void DocumentContextMenu::AddKindOption(document::DocumentKind kind, const QString& text) {
  auto* button = new QPushButton(QStringLiteral("    ") + text, this);
  button->setCursor(Qt::PointingHandCursor);
  button->setFocusPolicy(Qt::NoFocus);
  button->setFlat(true);

  if (auto* layout = qobject_cast<QVBoxLayout*>(this->layout())) {
    layout->addWidget(button);
  }

  connect(button, &QPushButton::clicked, this, [this, kind]() {
    close();
    emit newDocumentRequested(kind);
  });

  new_document_kind_buttons_.append(button);
}

}  // namespace cppwiki::gui
