pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Control {
    id: root

    property var model: ["Off", "Noise Cancellation", "Transparency", "Adaptive"]
    property int currentIndex: 0

    signal activated(int index)

    readonly property color trackBgColor: "#0f131d"
    readonly property color trackBorderColor: "#273146"
    readonly property color selectedBgColor: "#2563eb"
    readonly property color selectedBorderColor: "#3b82f6"
    readonly property color normalTextColor: "#94a3b8"
    readonly property color selectedTextColor: "#ffffff"

    padding: 3
    implicitHeight: 36
    implicitWidth: 320

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true

    background: Rectangle {
        radius: height / 2
        color: root.trackBgColor
        border.width: root.activeFocus ? 1.5 : 1
        border.color: root.activeFocus ? root.selectedBorderColor : root.trackBorderColor
    }

    contentItem: Row {
        id: buttonRow
        spacing: 3
        anchors.fill: parent
        anchors.margins: root.padding

        Repeater {
            id: buttonRepeater
            model: root.model

            delegate: Button {
                id: segmentButton
                required property int index
                required property string modelData
                text: modelData

                width: (buttonRow.width - (root.model.length - 1) * buttonRow.spacing) / root.model.length
                height: buttonRow.height
                focusPolicy: Qt.NoFocus

                contentItem: Text {
                    text: segmentButton.text
                    font.pixelSize: segmentButton.width < 60 ? 9 : (segmentButton.width < 75 ? 10 : 11)
                    font.weight: root.currentIndex === segmentButton.index ? Font.Bold : Font.Normal
                    color: root.currentIndex === segmentButton.index ? root.selectedTextColor : (segmentButton.hovered ? "#f1f5f9" : root.normalTextColor)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: height / 2
                    color: root.currentIndex === segmentButton.index ? root.selectedBgColor : (segmentButton.hovered ? "#182030" : "transparent")
                    border.width: root.currentIndex === segmentButton.index ? 1 : 0
                    border.color: root.selectedBorderColor

                    Behavior on color {
                        ColorAnimation { duration: 180; easing.type: Easing.OutQuad }
                    }
                }

                onClicked: {
                    if (root.currentIndex !== index) {
                        root.currentIndex = index;
                    }
                    root.activated(index);
                }
            }
        }
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Left) {
            if (root.currentIndex > 0) {
                root.currentIndex--;
                root.activated(root.currentIndex);
                event.accepted = true;
            }
        } else if (event.key === Qt.Key_Right) {
            if (root.currentIndex < root.model.length - 1) {
                root.currentIndex++;
                root.activated(root.currentIndex);
                event.accepted = true;
            }
        } else if (event.key === Qt.Key_Home) {
            root.currentIndex = 0;
            root.activated(0);
            event.accepted = true;
        } else if (event.key === Qt.Key_End) {
            root.currentIndex = root.model.length - 1;
            root.activated(root.currentIndex);
            event.accepted = true;
        } else if (event.key >= Qt.Key_1 && event.key <= Qt.Key_9) {
            const index = event.key - Qt.Key_1;
            if (index < root.model.length) {
                root.currentIndex = index;
                root.activated(index);
                event.accepted = true;
            }
        }
    }
}
