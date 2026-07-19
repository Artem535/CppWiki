#include "gui/document_context_menu.h"

#include <QIcon>
#include <QMenu>
#include <QPushButton>
#include <QSize>
#include <QStyle>
#include <QVBoxLayout>

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

  AddNewDocumentSubmenuButton();
  AddButton(Action::kRenameTitle, QStringLiteral("Rename title"),
            QIcon::fromTheme(QStringLiteral("document-edit")), true);
  AddButton(Action::kMoveUp, QStringLiteral("Move up"),
            style()->standardIcon(QStyle::SP_ArrowUp), options.can_move_up);
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

void DocumentContextMenu::AddNewDocumentSubmenuButton() {
  auto* button = new QPushButton(QStringLiteral("New document"), this);
  button->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
  button->setIconSize(QSize(16, 16));
  button->setCursor(Qt::PointingHandCursor);
  button->setFocusPolicy(Qt::NoFocus);
  button->setFlat(true);

  if (auto* layout = qobject_cast<QVBoxLayout*>(this->layout())) {
    layout->addWidget(button);
  }

  connect(button, &QPushButton::clicked, this,
          [this, button]() { ShowNewDocumentSubmenu(button); });
}

void DocumentContextMenu::ShowNewDocumentSubmenu(QPushButton* anchor) {
  auto* submenu = new QMenu(this);
  submenu->setAttribute(Qt::WA_DeleteOnClose);

  // "Wiki page" is first/default and, when chosen, must produce byte-identical
  // behavior to the old single hardcoded "Add child page" action.
  auto* wiki_page_action = submenu->addAction(QStringLiteral("Wiki page"));
  auto* jupyter_notebook_action = submenu->addAction(QStringLiteral("Jupyter notebook"));
  auto* excalidraw_canvas_action = submenu->addAction(QStringLiteral("Excalidraw canvas"));

  connect(wiki_page_action, &QAction::triggered, this, [this]() {
    close();
    emit newDocumentRequested(document::DocumentKind::kWikiPage);
  });
  connect(jupyter_notebook_action, &QAction::triggered, this, [this]() {
    close();
    emit newDocumentRequested(document::DocumentKind::kJupyterNotebook);
  });
  connect(excalidraw_canvas_action, &QAction::triggered, this, [this]() {
    close();
    emit newDocumentRequested(document::DocumentKind::kExcalidrawCanvas);
  });

  submenu->popup(anchor->mapToGlobal(QPoint(anchor->width(), 0)));
}

}  // namespace cppwiki::gui
