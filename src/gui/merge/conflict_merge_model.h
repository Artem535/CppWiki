#ifndef CPPWIKI_SRC_GUI_MERGE_CONFLICT_MERGE_MODEL_H_
#define CPPWIKI_SRC_GUI_MERGE_CONFLICT_MERGE_MODEL_H_

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "storage/local_document_repository.h"

namespace cppwiki::gui::merge {

class ConflictMergeModel final : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Role {
    BlockIdRole = Qt::UserRole + 1,
    BlockTypeRole,
    BlockLabelRole,
    LocalPreviewRole,
    RemotePreviewRole,
    ResolutionRole,
    IsConflictRole,
    HasLocalRole,
    HasRemoteRole,
  };
  Q_ENUM(Role)

  explicit ConflictMergeModel(QObject* parent = nullptr);

  struct Row {
    QString block_id;
    QString block_type;
    QString block_label;
    QJsonObject local_block;
    QJsonObject remote_block;
    QString local_preview;
    QString remote_preview;
    QString resolution{"local"};
    bool is_conflict{false};
    bool has_local{false};
    bool has_remote{false};
  };

  [[nodiscard]] auto rowCount(const QModelIndex& parent = QModelIndex()) const -> int override;
  [[nodiscard]] auto data(const QModelIndex& index, int role = Qt::DisplayRole) const
      -> QVariant override;
  [[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override;

  [[nodiscard]] auto LoadConflict(const storage::DocumentConflictRecord& conflict,
                                 const std::optional<storage::DocumentRecord>& current_document)
      -> bool;
  [[nodiscard]] auto HasContent() const -> bool;
  [[nodiscard]] auto ErrorMessage() const -> QString;
  [[nodiscard]] auto ConflictTitle() const -> QString;
  [[nodiscard]] auto WorkspaceId() const -> QString;
  [[nodiscard]] auto DocumentId() const -> QString;
  [[nodiscard]] auto BuildMergedDocument() const -> std::optional<QJsonObject>;

  Q_INVOKABLE void SetResolution(int row, const QString& resolution);
  Q_INVOKABLE void SetResolutionForAll(const QString& resolution);
  Q_INVOKABLE QString ResolutionForRow(int row) const;

 private:
  void Clear();
  void SetError(QString message);
  [[nodiscard]] auto BuildMergedBlocks() const -> QJsonArray;
  [[nodiscard]] auto PreferredBaseDocument() const -> QJsonObject;
  [[nodiscard]] auto PreferRemoteDocument() const -> bool;
  [[nodiscard]] auto PreferLocalDocument() const -> bool;
  [[nodiscard]] static auto PreviewTextForBlock(const QJsonObject& block) -> QString;
  [[nodiscard]] static auto BlockTypeForBlock(const QJsonObject& block) -> QString;
  [[nodiscard]] static auto BlockLabelForBlock(const QJsonObject& block, qsizetype row_index)
      -> QString;
  [[nodiscard]] static auto ParseSnapshotObject(const std::string& raw_snapshot_json,
                                                QString* error_message) -> std::optional<QJsonObject>;
  [[nodiscard]] static auto ExplicitBlockId(const QJsonObject& block) -> QString;
  [[nodiscard]] static auto BlockIdForBlock(const QJsonObject& block, qsizetype row_index)
      -> QString;
  [[nodiscard]] static auto BlocksFromSnapshot(const QJsonObject& snapshot)
      -> std::vector<QJsonObject>;
  [[nodiscard]] static auto BlocksEquivalent(const QJsonObject& lhs, const QJsonObject& rhs)
      -> bool;
  [[nodiscard]] static auto CloneBlockWithFreshId(QJsonObject block) -> QJsonObject;

  QString conflict_title_;
  QString workspace_id_;
  QString document_id_;
  QString error_message_;
  QJsonObject local_document_;
  QJsonObject remote_document_;
  std::vector<Row> rows_;
};

}  // namespace cppwiki::gui::merge

#endif  // CPPWIKI_SRC_GUI_MERGE_CONFLICT_MERGE_MODEL_H_
