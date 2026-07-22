import QtQuick 2.15

// A single draggable task card. Normally positioned by its parent cell via `visualIndex` (see
// KanbanColumnCell.qml). DropArea resolves drop targets by scene-coordinate overlap regardless of
// item-tree parentage, so dropping always works — but *painting* still follows tree/document
// order, so a card dragged down into a swimlane row that's painted after it would otherwise
// appear to slide underneath that row's cell background. To keep the drag visually correct across
// swimlanes (the main thing this component needs to demonstrate), the card reparents itself to
// `dragOverlay` — the board's top-level Item, painted last — for the duration of the gesture, and
// either gets destroyed (a fresh delegate appears in the right place once the model updates) or
// snaps back to its original parent/position if the drop didn't land on a DropArea.
Rectangle {
    id: card

    property var taskData
    readonly property string taskId: taskData.id
    property int visualIndex: 0
    // Taller than a bare priority+progress card to leave room for the tags row below the title
    // (see #131 follow-up: tags/assignee were dropped from the native card compared to the web
    // Kanban's cards). Kept in sync with KanbanColumnCell.qml's cardHeight.
    readonly property int cellHeight: 92
    // Top-level Item to reparent into while dragging, so the card paints above every swimlane
    // row rather than just above its own siblings. Passed down from KanbanBoard.qml.
    property Item dragOverlay: null

    width: 208
    height: 84
    x: 0
    y: visualIndex * cellHeight
    z: dragArea.drag.active ? 1000 : 1
    radius: 6
    // Colors from third_party/qlementine/showcase/resources/themes/dark.json.
    color: dragArea.drag.active ? "#333848" : "#282b33" // backgroundColorMain3 / backgroundColorMain2
    border.width: dragArea.drag.active ? 2 : 1
    border.color: dragArea.drag.active ? "#5086ff" : "#40485a" // primaryColor / borderColor

    Drag.active: dragArea.drag.active
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2
    Drag.source: card

    function priorityColor(priority) {
        if (priority === 3) return "#e96b72" // statusColorError
        if (priority === 2) return "#fbc064" // statusColorWarning
        if (priority === 1) return "#2bb5a0" // statusColorSuccess
        return "#4c5368" // neutralColor
    }

    Rectangle {
        id: priorityDot
        width: 10
        height: 10
        radius: 5
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 8
        color: card.priorityColor(card.taskData.priority)
    }

    // First assignee's initial as a small round badge, mirroring the priority dot on the other
    // corner -- a compact stand-in for the web card's assignee avatar (see #131 follow-up).
    Rectangle {
        id: assigneeBadge
        visible: card.taskData.users !== undefined && card.taskData.users.length > 0
        width: 18
        height: 18
        radius: 9
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        color: "#40485a" // borderColor

        Text {
            anchors.centerIn: parent
            text: visible && card.taskData.users.length > 0
                  ? card.taskData.users[0].charAt(0).toUpperCase() : ""
            font.pixelSize: 9
            font.bold: true
            color: "#e8eaf0"
        }
    }

    Text {
        id: taskText
        anchors.left: priorityDot.right
        anchors.right: assigneeBadge.left
        anchors.top: parent.top
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 6
        text: card.taskData.text
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
        maximumLineCount: 2
        font.pixelSize: 12
        color: "#e8eaf0" // light text, readable against the dark card background
    }

    // Tag chips, mirroring the web card's tag pills (see #131 follow-up).
    Row {
        id: tagsRow
        visible: card.taskData.tags !== undefined && card.taskData.tags.length > 0
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.top: taskText.bottom
        anchors.topMargin: 4
        spacing: 4
        clip: true
        width: parent.width - 16

        Repeater {
            model: visible ? card.taskData.tags : []
            delegate: Rectangle {
                radius: 3
                height: 16
                width: tagLabel.width + 10
                color: "#333848" // backgroundColorMain3
                border.width: 1
                border.color: "#40485a" // borderColor

                Text {
                    id: tagLabel
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: 9
                    color: "#c7cad6"
                }
            }
        }
    }

    Rectangle {
        id: progressTrack
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        height: 4
        radius: 2
        color: "#40485a" // borderColor

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * Math.max(0, Math.min(100, card.taskData.progress)) / 100
            radius: 2
            color: "#2bb5a0" // statusColorSuccess
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        drag.target: card
        cursorShape: Qt.OpenHandCursor
        onDoubleClicked: kanbanModel.requestEditTask(card.taskId)

        property Item homeParent: null
        property real homeX: 0
        property real homeY: 0

        onPressed: {
            homeParent = card.parent
            homeX = card.x
            homeY = card.y
            if (card.dragOverlay && card.dragOverlay !== card.parent) {
                var scenePos = card.mapToItem(card.dragOverlay, 0, 0)
                card.parent = card.dragOverlay
                card.x = scenePos.x
                card.y = scenePos.y
            }
        }

        onReleased: {
            var action = card.Drag.drop()
            if (action === Qt.MoveAction) {
                // The model just changed; kanbanModel.rows will be rebuilt and a fresh card
                // delegate will appear wherever it landed. This floating instance is now stale.
                card.destroy()
            } else if (card.parent !== homeParent) {
                // Drop missed every DropArea — return to where it started.
                card.parent = homeParent
                card.x = homeX
                card.y = homeY
            } else {
                card.x = 0
                card.y = card.visualIndex * card.cellHeight
            }
        }
    }
}
