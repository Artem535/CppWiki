import QtQuick 2.15
import QtQuick.Controls 2.15

// Root of the native Kanban board. Renders one shared column grid, repeated once per swimlane
// (epics first, "No epic" last) — NOT one stacked board per epic like the web/SVAR version. Both
// the swimlane grouping and the per-cell card lists come from `kanbanModel` (a KanbanBoardModel*
// set as a QML context property by KanbanBoardWidget); this file is purely presentation plus the
// drag gesture (see KanbanCard.qml / KanbanColumnCell.qml).
Rectangle {
    id: root
    color: "#fafbfc"

    readonly property int columnWidth: 220
    readonly property int columnSpacing: 10

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: mainColumn.width
        contentHeight: mainColumn.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}

        Column {
            id: mainColumn
            spacing: 20

            // Column header row, shared across every swimlane below it.
            Row {
                spacing: root.columnSpacing
                Repeater {
                    model: kanbanModel.columns
                    delegate: Rectangle {
                        width: root.columnWidth
                        height: 30
                        radius: 4
                        color: "#37474f"
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }
                }
            }

            // One row per swimlane (epic, or "No epic"), each repeating the same set of columns.
            Repeater {
                model: kanbanModel.rows
                delegate: Column {
                    id: swimlaneRow
                    readonly property var rowData: modelData
                    spacing: 6

                    Text {
                        height: 20
                        verticalAlignment: Text.AlignVCenter
                        text: swimlaneRow.rowData.swimlaneId === ""
                              ? swimlaneRow.rowData.swimlaneLabel
                              : "◆ " + swimlaneRow.rowData.swimlaneLabel
                        font.bold: true
                        font.pixelSize: 13
                        color: "#455a64"
                    }

                    Row {
                        spacing: root.columnSpacing
                        Repeater {
                            model: swimlaneRow.rowData.columns
                            delegate: KanbanColumnCell {
                                width: root.columnWidth
                                columnId: modelData.columnId
                                swimlaneId: swimlaneRow.rowData.swimlaneId
                                cards: modelData.cards
                                dragOverlay: root
                            }
                        }
                    }
                }
            }
        }
    }
}
