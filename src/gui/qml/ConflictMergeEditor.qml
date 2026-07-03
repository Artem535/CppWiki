import QtQuick 2.15

Rectangle {
    id: root
    color: "#20242b"
    radius: 14

    property color panelColor: "#2a2f37"
    property color panelBorder: "#39424d"
    property color accent: "#63b3ff"
    property color accentStrong: "#8cc8ff"
    property color danger: "#ff7a7a"
    property color success: "#79d2a6"
    property color textPrimary: "#e6edf3"
    property color textSecondary: "#9aa7b4"
    property color mutedBg: "#171a1f"

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Rectangle {
            width: parent.width
            height: 76
            color: panelColor
            radius: 12
            border.color: panelBorder
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6

                Text {
                    width: parent.width
                    text: mergeDialog.conflictTitle
                    color: textPrimary
                    font.pixelSize: 18
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: mergeDialog.statusMessage
                    color: mergeDialog.busy ? accentStrong : textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle {
            width: parent.width
            height: parent.height - 182
            color: mutedBg
            radius: 12
            border.color: panelBorder
            border.width: 1

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                spacing: 10
                model: mergeModel

                delegate: Rectangle {
                    width: list.width
                    height: 202
                    radius: 10
                    color: isConflict ? "#252b33" : "#21262d"
                    border.width: 1
                    border.color: resolution === "local" ? success : (resolution === "remote" ? danger : panelBorder)

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        Row {
                            width: parent.width
                            spacing: 10

                            Column {
                                width: parent.width - 220
                                spacing: 2

                                Text {
                                    width: parent.width
                                    text: blockLabel
                                    color: textPrimary
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: blockType + (isConflict ? "  conflict" : "  synced")
                                    color: textSecondary
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                width: 72
                                height: 28
                                radius: 14
                                color: resolution === "local" ? "#234235" : (resolution === "remote" ? "#41252a" : "#343a42")
                                border.width: 1
                                border.color: resolution === "local" ? success : (resolution === "remote" ? danger : panelBorder)

                                Text {
                                    anchors.centerIn: parent
                                    text: resolution === "local" ? "Local" : (resolution === "remote" ? "Remote" : "Both")
                                    color: textPrimary
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                        }

                        Row {
                            width: parent.width
                            height: 112
                            spacing: 10

                            Rectangle {
                                width: (parent.width - 20) / 2
                                height: parent.height
                                radius: 8
                                color: "#1a1f25"
                                border.color: panelBorder
                                border.width: 1

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    Text {
                                        text: "Local"
                                        color: success
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    Text {
                                        width: parent.width
                                        text: localPreview
                                        color: textPrimary
                                        font.pixelSize: 11
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 4
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Rectangle {
                                width: (parent.width - 20) / 2
                                height: parent.height
                                radius: 8
                                color: "#1a1f25"
                                border.color: panelBorder
                                border.width: 1

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    Text {
                                        text: "Remote"
                                        color: danger
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    Text {
                                        width: parent.width
                                        text: remotePreview
                                        color: textPrimary
                                        font.pixelSize: 11
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 4
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        Row {
                            spacing: 8

                            Rectangle {
                                width: 86
                                height: 28
                                radius: 14
                                color: resolution === "local" ? "#2d6848" : "#323a44"
                                border.width: 1
                                border.color: resolution === "local" ? success : panelBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "Use local"
                                    color: textPrimary
                                    font.pixelSize: 11
                                    font.bold: true
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: mergeDialog.setResolution(index, "local")
                                }
                            }

                            Rectangle {
                                width: 92
                                height: 28
                                radius: 14
                                color: resolution === "remote" ? "#783540" : "#323a44"
                                border.width: 1
                                border.color: resolution === "remote" ? danger : panelBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "Use remote"
                                    color: textPrimary
                                    font.pixelSize: 11
                                    font.bold: true
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: mergeDialog.setResolution(index, "remote")
                                }
                            }

                            Rectangle {
                                width: 78
                                height: 28
                                radius: 14
                                color: resolution === "both" ? "#4a4a2a" : "#323a44"
                                border.width: 1
                                border.color: resolution === "both" ? accent : panelBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: "Keep both"
                                    color: textPrimary
                                    font.pixelSize: 11
                                    font.bold: true
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: mergeDialog.setResolution(index, "both")
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 44
            color: panelColor
            radius: 12
            border.color: panelBorder
            border.width: 1

            Row {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Rectangle {
                    width: 110
                    height: parent.height - 20
                    radius: 14
                    color: "#2d6848"
                    border.color: success
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Use all local"
                        color: textPrimary
                        font.pixelSize: 11
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mergeDialog.useAllLocal()
                    }
                }

                Rectangle {
                    width: 118
                    height: parent.height - 20
                    radius: 14
                    color: "#783540"
                    border.color: danger
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Use all remote"
                        color: textPrimary
                        font.pixelSize: 11
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mergeDialog.useAllRemote()
                    }
                }

                Rectangle {
                    width: 112
                    height: parent.height - 20
                    radius: 14
                    color: "#2b6bb0"
                    border.color: accent
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Save merge"
                        color: textPrimary
                        font.pixelSize: 11
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mergeDialog.saveMerge()
                    }
                }

                Rectangle {
                    width: 88
                    height: parent.height - 20
                    radius: 14
                    color: "#31363f"
                    border.color: panelBorder
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Cancel"
                        color: textPrimary
                        font.pixelSize: 11
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mergeDialog.cancelMerge()
                    }
                }
            }
        }
    }
}
