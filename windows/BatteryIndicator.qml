import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property int batteryLevel: 50 // 0-100
    property bool isCharging: false
    property string indicator: "" // "L" or "R"

    readonly property color batteryLowColor: "#ef4444"
    readonly property color batteryMediumColor: "#facc15"
    readonly property color batteryHighColor: "#22c55e"
    readonly property color chargingColor: "#22c55e"
    readonly property color borderColor: "#475569"
    readonly property color textColor: "#f8fafc"
    readonly property color indicatorBgColor: "#242e42"
    readonly property color indicatorTextColor: "#93c5fd"

    // Size parameters
    width: 85
    height: 42
    color: "transparent"

    readonly property color levelColor: {
        if (isCharging) return chargingColor;
        if (batteryLevel <= 20) return batteryLowColor;
        if (batteryLevel <= 50) return batteryMediumColor;
        return batteryHighColor;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 5

        // Battery icon
        Item {
            id: batteryIcon
            Layout.preferredWidth: 32
            Layout.preferredHeight: 16
            Layout.alignment: Qt.AlignHCenter

            // Main battery body
            Rectangle {
                id: batteryBody
                width: parent.width - 2
                height: parent.height
                radius: 3
                color: Qt.rgba(0.08, 0.1, 0.15, 0.6)
                border.width: 1.5
                border.color: root.borderColor

                // Battery level fill
                Rectangle {
                    id: batteryFill
                    width: Math.max(2, (batteryBody.width - 4) * (Math.min(100, Math.max(0, root.batteryLevel)) / 100))
                    height: batteryBody.height - 4
                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 1.5
                    color: root.levelColor

                    Behavior on width {
                        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                    }

                    SequentialAnimation {
                        running: root.isCharging
                        loops: Animation.Infinite
                        alwaysRunToEnd: true
                        NumberAnimation { target: batteryFill; property: "opacity"; to: 0.6; duration: 1500 }
                        NumberAnimation { target: batteryFill; property: "opacity"; to: 1.0; duration: 1500 }
                    }
                }
            }

            // Battery positive terminal
            Rectangle {
                width: 2.5
                height: 8
                radius: 1
                color: root.borderColor
                anchors.left: batteryBody.right
                anchors.leftMargin: 0.5
                anchors.verticalCenter: batteryBody.verticalCenter
            }

            // Charging lightning bolt
            Canvas {
                id: chargingBolt
                visible: root.isCharging
                width: 14
                height: 14
                anchors.centerIn: batteryBody

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.fillStyle = "#ffffff";
                    ctx.beginPath();
                    ctx.moveTo(7, 2);
                    ctx.lineTo(3, 8);
                    ctx.lineTo(6, 8);
                    ctx.lineTo(5, 12);
                    ctx.lineTo(11, 6);
                    ctx.lineTo(8, 6);
                    ctx.lineTo(9, 2);
                    ctx.closePath();
                    ctx.fill();
                }
            }
        }

        // Percentage and L/R label
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 5

            // Left/Right indicator badge
            Rectangle {
                id: indicatorBadge
                visible: root.indicator !== ""
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                radius: 8
                color: root.indicatorBgColor
                border.color: "#3b82f6"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: root.indicator
                    color: root.indicatorTextColor
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            // Battery percentage text
            Text {
                id: percentageText
                text: root.batteryLevel + "%"
                color: root.textColor
                font.pixelSize: 12
                font.bold: true
            }
        }
    }
}
