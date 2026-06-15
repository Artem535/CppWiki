#ifndef CPPWIKI_EDITOR_BRIDGE_H
#define CPPWIKI_EDITOR_BRIDGE_H

#include <QObject>
#include <QString>
#include <QVariant>

namespace cppwiki::bridge {

class QEditorBridge final : public QObject {
  Q_OBJECT

 public:
  explicit QEditorBridge(QObject* parent = nullptr);

  Q_INVOKABLE static QVariantMap getBridgeInfo();
  Q_INVOKABLE static QVariantMap getInitialDocument();
  Q_INVOKABLE QVariantMap updateSnapshot(const QString& snapshot_json);
};

}  // namespace cppwiki::bridge

#endif  // CPPWIKI_EDITOR_BRIDGE_H
