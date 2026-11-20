#include "plugins/surface_renderer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <cmath>
#include <string>
#include <utility>

namespace cppwiki::plugins {

namespace {

auto ToQString(const std::string& value) -> QString {
  return QString::fromStdString(value);
}

void AddChildren(const SurfaceNode& node, QWidget* parent, QVBoxLayout* layout,
                 const std::function<QWidget*(const SurfaceNode&, QWidget*)>& build) {
  for (const auto& child : node.children) {
    if (auto* widget = build(child, parent); widget != nullptr) {
      layout->addWidget(widget);
    }
  }
}

}  // namespace

auto QtSurfaceRenderer::Render(const SurfaceNode& root, const SurfaceRenderContext& context,
                               QWidget* parent) -> QWidget* {
  ClearFields();
  context_ = context;
  return Build(root, parent);
}

auto QtSurfaceRenderer::Build(const SurfaceNode& node, QWidget* parent) -> QWidget* {
  const auto build = [this](const SurfaceNode& child, QWidget* child_parent) {
    return Build(child, child_parent);
  };

  if (node.type == "panel") {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    AddChildren(node, widget, layout, build);
    return widget;
  }
  if (node.type == "section") {
    auto* group = new QGroupBox(ToQString(node.title), parent);
    auto* layout = new QVBoxLayout(group);
    AddChildren(node, group, layout, build);
    return group;
  }
  if (node.type == "text") {
    auto* label = new QLabel(ToQString(node.text), parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
  }
  if (node.type == "markdown") {
    auto* label = new QLabel(ToQString(node.text), parent);
    label->setTextFormat(Qt::MarkdownText);
    label->setWordWrap(true);
    label->setOpenExternalLinks(false);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
  }
  if (node.type == "list") {
    auto* list = new QListWidget(parent);
    for (const auto& item : node.items) {
      list->addItem(ToQString(item));
    }
    list->setSelectionMode(QAbstractItemView::NoSelection);
    return list;
  }
  if (node.type == "table") {
    auto* table = new QTableWidget(static_cast<int>(node.rows.size()),
                                   static_cast<int>(node.columns.size()), parent);
    QStringList headers;
    headers.reserve(static_cast<qsizetype>(node.columns.size()));
    for (const auto& column : node.columns) {
      headers.push_back(ToQString(column));
    }
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (std::size_t row = 0; row < node.rows.size(); ++row) {
      for (std::size_t column = 0; column < node.rows[row].size(); ++column) {
        table->setItem(static_cast<int>(row), static_cast<int>(column),
                       new QTableWidgetItem(ToQString(node.rows[row][column])));
      }
    }
    table->resizeColumnsToContents();
    return table;
  }
  if (node.type == "key-value") {
    auto* widget = new QWidget(parent);
    auto* form = new QFormLayout(widget);
    form->setContentsMargins(0, 0, 0, 0);
    for (const auto& [key, value] : node.key_values) {
      auto* value_label = new QLabel(ToQString(value), widget);
      value_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
      form->addRow(ToQString(key), value_label);
    }
    return widget;
  }
  if (node.type == "form") {
    return BuildForm(node, parent);
  }
  if (node.type == "actions") {
    return BuildActions(node, parent);
  }
  if (node.type == "progress") {
    auto* widget = new QWidget(parent);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    if (!node.label.empty()) {
      layout->addWidget(new QLabel(ToQString(node.label), widget));
    }
    auto* bar = new QProgressBar(widget);
    bar->setRange(0, 100);
    bar->setValue(static_cast<int>(std::lround(node.progress * 100.0)));
    layout->addWidget(bar);
    return widget;
  }
  if (node.type == "badge") {
    auto* label = new QLabel(ToQString(node.text), parent);
    label->setProperty("surfaceRole", QStringLiteral("badge"));
    label->setAlignment(Qt::AlignCenter);
    return label;
  }

  auto* fallback = new QLabel(ToQString("Unsupported surface: " + node.type), parent);
  fallback->setWordWrap(true);
  return fallback;
}

auto QtSurfaceRenderer::BuildForm(const SurfaceNode& node, QWidget* parent) -> QWidget* {
  auto* widget = new QWidget(parent);
  auto* layout = new QVBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);

  auto* form = new QFormLayout();
  for (const auto& field : node.fields) {
    const auto object_name = QStringLiteral("surface-field-") + ToQString(field.name);
    QWidget* editor = nullptr;
    std::function<std::string()> getter;

    if (field.type == "text") {
      auto* edit = new QLineEdit(ToQString(field.value), widget);
      edit->setObjectName(object_name);
      editor = edit;
      getter = [edit] { return edit->text().toStdString(); };
    } else if (field.type == "number") {
      auto* spin = new QDoubleSpinBox(widget);
      spin->setObjectName(object_name);
      spin->setRange(-1.0e12, 1.0e12);
      spin->setDecimals(2);
      spin->setValue(ToQString(field.value).toDouble());
      editor = spin;
      getter = [spin] { return QString::number(spin->value(), 'g', 15).toStdString(); };
    } else if (field.type == "boolean") {
      auto* check = new QCheckBox(widget);
      check->setObjectName(object_name);
      check->setChecked(field.value == "true" || field.value == "1");
      editor = check;
      getter = [check] { return check->isChecked() ? "true" : "false"; };
    } else if (field.type == "select") {
      auto* combo = new QComboBox(widget);
      combo->setObjectName(object_name);
      for (const auto& option : field.options) {
        combo->addItem(ToQString(option));
      }
      combo->setCurrentText(ToQString(field.value));
      editor = combo;
      getter = [combo] { return combo->currentText().toStdString(); };
    } else if (field.type == "multi-select") {
      auto* list = new QListWidget(widget);
      list->setObjectName(object_name);
      list->setSelectionMode(QAbstractItemView::ExtendedSelection);
      for (const auto& option : field.options) {
        list->addItem(ToQString(option));
      }
      const QString initial = ToQString(field.value);
      for (const auto& selected : initial.split(',', Qt::SkipEmptyParts)) {
        const auto matches = list->findItems(selected.trimmed(), Qt::MatchExactly);
        for (auto* item : matches) {
          item->setSelected(true);
        }
      }
      editor = list;
      getter = [list] {
        std::string value;
        for (const auto* item : list->selectedItems()) {
          if (!value.empty()) {
            value += ",";
          }
          value += item->text().toStdString();
        }
        return value;
      };
    }

    if (editor != nullptr) {
      form->addRow(ToQString(field.label), editor);
      fields_.emplace_back(field.name, std::move(getter));
    }
  }
  layout->addLayout(form);

  const auto build = [this](const SurfaceNode& child, QWidget* child_parent) {
    return Build(child, child_parent);
  };
  for (const auto& child : node.children) {
    if (auto* child_widget = build(child, widget); child_widget != nullptr) {
      layout->addWidget(child_widget);
    }
  }
  return widget;
}

auto QtSurfaceRenderer::BuildActions(const SurfaceNode& node, QWidget* parent) -> QWidget* {
  auto* widget = new QWidget(parent);
  auto* layout = new QHBoxLayout(widget);
  layout->setContentsMargins(0, 0, 0, 0);
  for (const auto& action : node.actions) {
    auto* button = new QPushButton(ToQString(action.label), widget);
    button->setObjectName(QStringLiteral("surface-action-") + ToQString(action.id));
    if (action.style == "primary") {
      button->setProperty("surfaceRole", QStringLiteral("primary"));
    }
    const auto action_id = action.id;
    QObject::connect(button, &QPushButton::clicked, widget, [this, action_id] {
      if (!context_.on_event) {
        return;
      }
      SurfaceEvent event;
      event.surface_id = context_.surface_id;
      event.action_id = action_id;
      event.values = CollectValues();
      context_.on_event(event);
    });
    layout->addWidget(button);
  }
  return widget;
}

auto QtSurfaceRenderer::CollectValues() const -> std::map<std::string, std::string> {
  std::map<std::string, std::string> values;
  for (const auto& [name, getter] : fields_) {
    values[name] = getter();
  }
  return values;
}

void QtSurfaceRenderer::ClearFields() {
  fields_.clear();
}

}  // namespace cppwiki::plugins
