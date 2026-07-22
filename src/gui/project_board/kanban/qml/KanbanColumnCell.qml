import QtQuick 2.15

// One (swimlane x column) cell: a drop target for cards plus the vertical stack of cards
// currently in it. `cards` is the pre-filtered QVariantList the C++ model already computed for
// this exact (swimlaneId, columnId) pair (see KanbanBoardModel::rows()) — the QML side never has
// to filter the full task list itself.
Rectangle {
    id: cell

    property string columnId
    property string swimlaneId
    property var cards: []
    // Forwarded down to each card so it can reparent itself above every swimlane row while being
    // dragged (see KanbanCard.qml).
    property Item dragOverlay: null

    readonly property int cardHeight: 78

    color: dropArea.containsDrag ? "#e3f2fd" : "#f7f8fa"
    border.width: 1
    border.color: dropArea.containsDrag ? "#63b3ff" : "#e0e5ea"
    radius: 4
    height: Math.max(56, cards.length * cardHeight + 12)

    Behavior on color { ColorAnimation { duration: 100 } }

    DropArea {
        id: dropArea
        anchors.fill: parent

        onDropped: function(drop) {
            kanbanModel.moveCard(drop.source.taskId, cell.columnId, cell.swimlaneId)
            drop.accept(Qt.MoveAction)
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 6

        Repeater {
            model: cell.cards
            delegate: KanbanCard {
                taskData: modelData
                visualIndex: index
                dragOverlay: cell.dragOverlay
            }
        }
    }
}
