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

    // Colors from third_party/qlementine/showcase/resources/themes/dark.json.
    color: dropArea.containsDrag ? "#2c3448" : "#282b33" // primaryColorDisabled / backgroundColorMain2
    border.width: 1
    border.color: dropArea.containsDrag ? "#5086ff" : "#40485a" // primaryColor / borderColor
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
