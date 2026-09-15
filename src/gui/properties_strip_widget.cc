#include "gui/properties_strip_widget.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLayoutItem>
#include <QSize>
#include <QStringList>
#include <QToolButton>
#include <QWidget>

#include "knowledge/knowledge_record.h"
#include "storage/local_document_repository.h"

namespace cppwiki::gui {

namespace {

constexpr std::array<const char*, 9> kTagPalette = {
    "grey", "red", "orange", "yellow", "green", "teal", "blue", "purple", "pink",
};

// Mirrors frontend/editor/src/properties/propertyModel.ts's colorForTagValue exactly (same
// 32-bit wraparound hash, same palette order) so a property value reads as the same color here
// and in the JS-side drawer's pills, even though the two are rendered by entirely different UI
// toolkits.
QString ColorNameForTagValue(const QString& value) {
  std::uint32_t hash = 0;
  for (const QChar ch : value) {
    hash = hash * 31u + static_cast<std::uint32_t>(ch.unicode());
  }
  const auto signed_hash = static_cast<std::int32_t>(hash);
  const auto magnitude = static_cast<std::size_t>(std::llabs(static_cast<long long>(signed_hash)));
  return QString::fromLatin1(kTagPalette[magnitude % kTagPalette.size()]);
}

bool UsesPillColor(knowledge::PropertyValueKind kind) {
  return kind == knowledge::PropertyValueKind::kSelect ||
        kind == knowledge::PropertyValueKind::kMultiSelect ||
        kind == knowledge::PropertyValueKind::kTags;
}

}  // namespace

PropertiesStripWidget::PropertiesStripWidget(QWidget* parent) : QFrame(parent) {
  setObjectName(QStringLiteral("propertiesStripWidget"));
  setFrameShape(QFrame::NoFrame);

  root_layout_ = new QHBoxLayout(this);
  root_layout_->setContentsMargins(0, 0, 0, 0);
  root_layout_->setSpacing(6);

  Rebuild();
}

void PropertiesStripWidget::SetRepository(
    std::shared_ptr<storage::LocalDocumentRepository> repository) {
  repository_ = std::move(repository);
  Rebuild();
}

void PropertiesStripWidget::SetDocumentContext(const QString& workspace_id, const QString& page_id,
                                               bool has_document) {
  workspace_id_ = workspace_id;
  page_id_ = page_id;
  has_document_ = has_document;
  Rebuild();
}

void PropertiesStripWidget::RefreshCurrent() { Rebuild(); }

QWidget* PropertiesStripWidget::CreateChip(const QString& name, const QString& value,
                                           const QString& color_name) {
  // Fixed height (not fixed width -- text length varies) is the uniformity PresenceStripWidget's
  // avatars already have and this row previously lacked when it lived in the web layer as
  // differently-sized <select>/<input> elements.
  auto* chip = new QWidget(this);
  chip->setObjectName(QStringLiteral("propertyChip"));
  chip->setFixedHeight(22);

  auto* layout = new QHBoxLayout(chip);
  layout->setContentsMargins(8, 0, 8, 0);
  layout->setSpacing(4);

  auto* name_label = new QLabel(name, chip);
  name_label->setObjectName(QStringLiteral("propertyChipName"));
  layout->addWidget(name_label);

  auto* value_label = new QLabel(value, chip);
  value_label->setObjectName(QStringLiteral("propertyChipValue"));
  if (!color_name.isEmpty()) {
    value_label->setProperty("chipColor", color_name);
    // QLabel doesn't paint a stylesheet background/border-radius by default; without this the
    // [chipColor=...] rules below only ever change the text color, never the pill background.
    value_label->setAttribute(Qt::WA_StyledBackground, true);
  }
  layout->addWidget(value_label);

  return chip;
}

void PropertiesStripWidget::Rebuild() {
  while (root_layout_->count() > 0) {
    auto* item = root_layout_->takeAt(0);
    if (item == nullptr) {
      continue;
    }
    if (auto* widget = item->widget(); widget != nullptr) {
      widget->deleteLater();
    }
    delete item;
  }

  if (!has_document_ || !repository_) {
    root_layout_->addStretch(0);
    return;
  }

  const auto definitions = repository_->ListPropertyDefinitions(workspace_id_.toStdString());
  const auto values =
      repository_->ListPagePropertyValues(workspace_id_.toStdString(), page_id_.toStdString());
  if (definitions.error || values.error) {
    root_layout_->addStretch(0);
    return;
  }

  int rendered_count = 0;
  int available_count = 0;
  for (const auto& definition : definitions.definitions) {
    if (definition.state != knowledge::RecordState::kActive) {
      continue;
    }
    const auto found =
        std::find_if(values.values.begin(), values.values.end(), [&](const auto& value) {
          return value.property_definition_id == definition.id;
        });
    if (found == values.values.end() || found->values.empty()) {
      continue;
    }
    ++available_count;
    if (rendered_count >= kMaxVisibleChips) {
      continue;
    }

    QStringList raw_values;
    for (const auto& one : found->values) {
      raw_values << QString::fromStdString(one);
    }
    const auto color_name =
        UsesPillColor(definition.value_kind) ? ColorNameForTagValue(raw_values.first()) : QString();

    auto* chip = CreateChip(QString::fromStdString(definition.name),
                            raw_values.join(QStringLiteral(", ")), color_name);
    root_layout_->addWidget(chip, 0, Qt::AlignVCenter);
    ++rendered_count;
  }

  if (available_count > rendered_count) {
    auto* overflow = new QLabel(QStringLiteral("+%1").arg(available_count - rendered_count), this);
    overflow->setObjectName(QStringLiteral("propertyChipOverflow"));
    root_layout_->addWidget(overflow, 0, Qt::AlignVCenter);
  }

  auto* button = new QToolButton(this);
  button->setObjectName(QStringLiteral("propertiesStripButton"));
  button->setIcon(QIcon(QStringLiteral(":/cppwiki/icons/action-properties.svg")));
  button->setIconSize(QSize(16, 16));
  button->setAutoRaise(true);
  button->setToolTip(QStringLiteral("Open properties"));
  connect(button, &QToolButton::clicked, this, &PropertiesStripWidget::propertiesRequested);
  root_layout_->addWidget(button, 0, Qt::AlignVCenter);

  root_layout_->addStretch(0);
}

}  // namespace cppwiki::gui
