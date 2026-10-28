import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

Window {
    id: root
    title: "Magic Cloud Keys QR Code"
    flags: Qt.Dialog
    modality: Qt.WindowModal
    color: "#0f131c"

    width: Math.min(Screen.width * 0.8, 320)
    height: Math.min(Screen.height * 0.7, 380)

    property string irk: ""
    property string encKey: ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // QR Code Container
        Rectangle {
            id: qrContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: width
            radius: 12
            color: "#ffffff"
            border.color: "#38bdf8"
            border.width: 2

            Image {
                id: qrCodeImage
                anchors.centerIn: parent
                width: Math.min(parent.width * 0.88, parent.height * 0.88)
                height: width
                fillMode: Image.PreserveAspectFit
                source: "image://qrcode/" + root.encKey + ";" + root.irk

                BusyIndicator {
                    anchors.centerIn: parent
                    running: qrCodeImage.status === Image.Loading
                }

                Label {
                    anchors.centerIn: parent
                    visible: qrCodeImage.status === Image.Error
                    text: "Failed to generate QR code"
                    color: "#ef4444"
                }
            }
        }

        // Instruction text
        Label {
            Layout.fillWidth: true
            text: "Scan this QR code with LibrePods Android\nto sync your Magic Cloud Keys"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "#cbd5e1"
            font.pixelSize: 13
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: "Close"
            onClicked: root.close()
            contentItem: Text {
                text: "Close"
                color: "#ffffff"
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 100
                implicitHeight: 32
                radius: 6
                color: parent.hovered ? "#3b82f6" : "#2563eb"
            }
        }
    }
}