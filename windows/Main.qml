pragma ComponentBehavior: Bound

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: mainWindow
    visible: !airPodsTrayApp.hideOnStart
    minimumWidth: 240
    minimumHeight: 360
    width: Math.max(300, airPodsTrayApp.windowWidth() > 0 ? airPodsTrayApp.windowWidth() : 380)
    height: Math.max(400, airPodsTrayApp.windowHeight() > 0 ? airPodsTrayApp.windowHeight() : 480)
    x: (airPodsTrayApp.windowX() > 0) ? airPodsTrayApp.windowX() : undefined
    y: (airPodsTrayApp.windowY() > 0) ? airPodsTrayApp.windowY() : undefined
    title: "LibrePods"
    objectName: "mainWindowObject"
    color: "#0f131c"

    // Debounced timer to persist window dimensions & coordinates
    Timer {
        id: geometrySaveTimer
        interval: 600
        repeat: false
        onTriggered: {
            if (mainWindow.visibility !== Window.Minimized && mainWindow.width >= 300 && mainWindow.height >= 300) {
                airPodsTrayApp.saveWindowGeometry(mainWindow.x, mainWindow.y, mainWindow.width, mainWindow.height)
            }
        }
    }

    onWidthChanged: geometrySaveTimer.restart()
    onHeightChanged: geometrySaveTimer.restart()
    onXChanged: geometrySaveTimer.restart()
    onYChanged: geometrySaveTimer.restart()

    onClosing: {
        if (mainWindow.visibility !== Window.Minimized && mainWindow.width >= 300 && mainWindow.height >= 300) {
            airPodsTrayApp.saveWindowGeometry(mainWindow.x, mainWindow.y, mainWindow.width, mainWindow.height)
        }
        mainWindow.visible = false
    }

    function reopen(pageToLoad) {
        if (pageToLoad === "settings") {
            if (stackView.depth === 1) {
                stackView.push(settingsPage)
            }
        } else {
            if (stackView.depth > 1) {
                stackView.pop()
            }
        }

        if (!mainWindow.visible) {
            mainWindow.visible = true
        }
        raise()
        requestActivate()
    }

    // Mouse area for handling back/forward navigation
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        onClicked: (mouse) => {
            if (mouse.button === Qt.BackButton && stackView.depth > 1) {
                stackView.pop()
            }
        }
    }

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: mainPage
    }

    FontLoader {
        id: iconFont
        source: "qrc:/icons/assets/fonts/SF-Symbols-6.ttf"
    }

    // =========================================================================
    // INLINE REUSABLE STYLED CONTROLS (UNIFIED DARK THEME)
    // =========================================================================

    component AppSwitch: Switch {
        id: switchControl
        implicitWidth: 42
        implicitHeight: 24

        indicator: Rectangle {
            implicitWidth: 42
            implicitHeight: 24
            radius: 12
            color: switchControl.checked ? "#2563eb" : "#273146"
            border.color: switchControl.checked ? "#3b82f6" : "#3b4760"
            border.width: 1

            Rectangle {
                x: switchControl.checked ? parent.width - width - 2 : 2
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 20
                radius: 10
                color: "#ffffff"

                Behavior on x {
                    NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                }
            }
        }
    }

    component AppButton: Button {
        id: btn
        contentItem: Text {
            text: btn.text
            color: btn.highlighted ? "#ffffff" : (btn.hovered ? "#ffffff" : "#cbd5e1")
            font.pixelSize: 12
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            implicitHeight: 32
            implicitWidth: 78
            radius: 6
            color: btn.highlighted ? (btn.hovered ? "#3b82f6" : "#2563eb") : (btn.hovered ? "#242e42" : "#192234")
            border.color: btn.highlighted ? "#60a5fa" : (btn.hovered ? "#475569" : "#334155")
            border.width: 1
        }
    }

    component AppSlider: Slider {
        id: sliderControl
        background: Rectangle {
            x: sliderControl.leftPadding
            y: sliderControl.topPadding + sliderControl.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 4
            width: sliderControl.availableWidth
            height: implicitHeight
            radius: 2
            color: "#273248"

            Rectangle {
                width: sliderControl.visualPosition * parent.width
                height: parent.height
                color: "#38bdf8"
                radius: 2
            }
        }
        handle: Rectangle {
            x: sliderControl.leftPadding + sliderControl.visualPosition * (sliderControl.availableWidth - width)
            y: sliderControl.topPadding + sliderControl.availableHeight / 2 - height / 2
            implicitWidth: 18
            implicitHeight: 18
            radius: 9
            color: "#ffffff"
            border.color: "#38bdf8"
            border.width: 2
        }
    }

    component AppTextField: TextField {
        id: field
        color: "#f8fafc"
        placeholderTextColor: "#64748b"
        font.pixelSize: 12
        leftPadding: 10
        rightPadding: 10
        background: Rectangle {
            implicitHeight: 34
            radius: 6
            color: "#10141e"
            border.color: field.activeFocus ? "#3b82f6" : "#273146"
            border.width: 1
        }
    }

    component AppComboBox: ComboBox {
        id: combo
        contentItem: Text {
            leftPadding: 10
            rightPadding: combo.indicator.width + 10
            text: combo.displayText
            font.pixelSize: 12
            color: "#f8fafc"
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            implicitWidth: 140
            implicitHeight: 34
            radius: 6
            color: "#10141e"
            border.color: combo.activeFocus ? "#3b82f6" : "#273146"
            border.width: 1
        }
        popup: Popup {
            y: combo.height + 2
            width: combo.width
            implicitHeight: contentItem.implicitHeight + 8
            padding: 4
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator { }
            }
            background: Rectangle {
                radius: 8
                color: "#161b26"
                border.color: "#334155"
                border.width: 1
            }
        }
        delegate: ItemDelegate {
            width: combo.width - 8
            contentItem: Text {
                text: modelData
                color: highlighted ? "#38bdf8" : "#e2e8f0"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: highlighted ? "#222a3d" : "transparent"
                radius: 4
            }
            highlighted: combo.highlightedIndex === index
        }
    }

    component AppSpinBox: SpinBox {
        id: spin
        contentItem: TextInput {
            text: spin.textFromValue(spin.value, spin.locale)
            font.pixelSize: 12
            font.bold: true
            color: "#f8fafc"
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            readOnly: !spin.editable
            validator: spin.validator
            inputMethodHints: Qt.ImhFormattedNumbersOnly
        }
        background: Rectangle {
            implicitWidth: 110
            implicitHeight: 34
            radius: 6
            color: "#10141e"
            border.color: "#273146"
            border.width: 1
        }
        up.indicator: Rectangle {
            x: spin.mirrored ? 0 : spin.width - width
            height: spin.height
            implicitWidth: 28
            color: spin.up.pressed ? "#2e3b52" : (spin.up.hovered ? "#222d3e" : "transparent")
            Text {
                text: "+"
                font.pixelSize: 14
                color: "#cbd5e1"
                anchors.centerIn: parent
            }
        }
        down.indicator: Rectangle {
            x: spin.mirrored ? spin.width - width : 0
            height: spin.height
            implicitWidth: 28
            color: spin.down.pressed ? "#2e3b52" : (spin.down.hovered ? "#222d3e" : "transparent")
            Text {
                text: "-"
                font.pixelSize: 14
                color: "#cbd5e1"
                anchors.centerIn: parent
            }
        }
    }

    // =========================================================================
    // MAIN PAGE
    // =========================================================================
    Component {
        id: mainPage
        Page {
            background: Rectangle { color: "#0f131c" }

            ScrollView {
                id: mainScrollView
                anchors.fill: parent
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                // Responsive auto-scaling container:
                // When window width is narrower than baseWidth (380px),
                // the entire page layout scales down proportionally so all components remain fully visible.
                Item {
                    id: mainScaledWrapper
                    readonly property real baseWidth: 380
                    readonly property real uiScale: Math.min(1.0, mainScrollView.availableWidth / baseWidth)

                    width: mainScrollView.availableWidth
                    height: mainContentCol.implicitHeight * uiScale

                    Column {
                        id: mainContentCol
                        width: mainScaledWrapper.baseWidth
                        scale: mainScaledWrapper.uiScale
                        transformOrigin: Item.TopLeft
                        spacing: 14
                        padding: 16

                    // Top Bar (Header + Status + Settings Button)
                    RowLayout {
                        width: parent.width - 32
                        anchors.horizontalCenter: parent.horizontalCenter

                        Row {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            Label {
                                text: "LibrePods"
                                font.pixelSize: 18
                                font.bold: true
                                color: "#f8fafc"
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // Connection Badge Pill
                            Rectangle {
                                width: statusText.implicitWidth + 14
                                height: 22
                                radius: 11
                                color: airPodsTrayApp.airpodsConnected ? "#15803d" : "#b91c1c"
                                anchors.verticalCenter: parent.verticalCenter

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 5
                                    Rectangle {
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: "#ffffff"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Label {
                                        id: statusText
                                        text: airPodsTrayApp.airpodsConnected ? "Connected" : "Disconnected"
                                        color: "#ffffff"
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Settings Icon Button
                        RoundButton {
                            font.family: iconFont.name
                            font.pixelSize: 17
                            text: "\uf958"
                            palette.buttonText: "#cbd5e1"
                            background: Rectangle {
                                radius: 16
                                color: parent.hovered ? "#222a3d" : "#192030"
                                border.color: "#334155"
                                border.width: 1
                            }
                            onClicked: stackView.push(settingsPage)
                        }
                    }

                    // ---------------------------------------------------------
                    // 1. Hero Device & Battery Card
                    // ---------------------------------------------------------
                    Rectangle {
                        width: parent.width - 32
                        anchors.horizontalCenter: parent.horizontalCenter
                        implicitHeight: heroColumn.implicitHeight + 28
                        height: implicitHeight
                        radius: 12
                        color: "#161b26"
                        border.color: "#273146"
                        border.width: 1

                        Column {
                            id: heroColumn
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 14
                            width: parent.width - 24
                            spacing: 12

                            // Device & Model Titles
                            Column {
                                spacing: 2
                                anchors.horizontalCenter: parent.horizontalCenter

                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: airPodsTrayApp.deviceInfo.deviceName ? airPodsTrayApp.deviceInfo.deviceName : "AirPods Pro"
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: "#ffffff"
                                }
                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: airPodsTrayApp.deviceInfo.modelDisplayName + (airPodsTrayApp.deviceInfo.modelNumber ? " (" + airPodsTrayApp.deviceInfo.modelNumber + ")" : "")
                                    font.pixelSize: 12
                                    color: "#94a3b8"
                                }
                            }

                            // Subtle Divider
                            Rectangle {
                                width: parent.width
                                height: 1
                                color: "#222c3e"
                            }

                            // Pod & Case Battery Row (Scales cleanly when window width shrinks)
                            Item {
                                id: podRowContainer
                                width: parent.width
                                implicitHeight: podRow.height * podRow.scale
                                height: implicitHeight

                                Row {
                                    id: podRow
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Math.max(6, Math.min(14, (parent.width - 240) / 4))
                                    scale: Math.min(1.0, parent.width / 270)
                                    transformOrigin: Item.Center

                                    PodColumn {
                                        visible: airPodsTrayApp.deviceInfo.battery.leftPodAvailable
                                        inEar: airPodsTrayApp.deviceInfo.leftPodInEar
                                        iconSource: "qrc:/icons/assets/" + airPodsTrayApp.deviceInfo.podIcon
                                        batteryLevel: airPodsTrayApp.deviceInfo.battery.leftPodLevel
                                        isCharging: airPodsTrayApp.deviceInfo.battery.leftPodCharging
                                        indicator: "L"
                                    }

                                    PodColumn {
                                        visible: airPodsTrayApp.deviceInfo.battery.rightPodAvailable
                                        inEar: airPodsTrayApp.deviceInfo.rightPodInEar
                                        iconSource: "qrc:/icons/assets/" + airPodsTrayApp.deviceInfo.podIcon
                                        batteryLevel: airPodsTrayApp.deviceInfo.battery.rightPodLevel
                                        isCharging: airPodsTrayApp.deviceInfo.battery.rightPodCharging
                                        indicator: "R"
                                    }

                                    PodColumn {
                                        visible: airPodsTrayApp.deviceInfo.battery.caseAvailable
                                        inEar: true
                                        iconSource: "qrc:/icons/assets/" + airPodsTrayApp.deviceInfo.caseIcon
                                        batteryLevel: airPodsTrayApp.deviceInfo.battery.caseLevel
                                        isCharging: airPodsTrayApp.deviceInfo.battery.caseCharging
                                    }

                                    PodColumn {
                                        visible: airPodsTrayApp.deviceInfo.battery.headsetAvailable
                                        inEar: true
                                        iconSource: "qrc:/icons/assets/" + airPodsTrayApp.deviceInfo.podIcon
                                        batteryLevel: airPodsTrayApp.deviceInfo.battery.headsetLevel
                                        isCharging: airPodsTrayApp.deviceInfo.battery.headsetCharging
                                    }

                                    Label {
                                        visible: !airPodsTrayApp.deviceInfo.battery.leftPodAvailable && !airPodsTrayApp.deviceInfo.battery.headsetAvailable
                                        text: "Connect AirPods to display live battery status"
                                        color: "#64748b"
                                        font.pixelSize: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // 2. Noise Control Section Card
                    // ---------------------------------------------------------
                    Rectangle {
                        visible: airPodsTrayApp.airpodsConnected
                        width: parent.width - 32
                        anchors.horizontalCenter: parent.horizontalCenter
                        implicitHeight: noiseColumn.implicitHeight + 24
                        height: implicitHeight
                        radius: 12
                        color: "#161b26"
                        border.color: "#273146"
                        border.width: 1

                        Column {
                            id: noiseColumn
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 12
                            width: parent.width - 24
                            spacing: 12

                            Label {
                                text: "NOISE CONTROL"
                                font.pixelSize: 10
                                font.bold: true
                                color: "#60a5fa"
                            }

                            SegmentedControl {
                                width: parent.width
                                model: ["Off", "Noise Cancellation", "Transparency", "Adaptive"]
                                currentIndex: airPodsTrayApp.deviceInfo.noiseControlMode
                                onActivated: function(index) { airPodsTrayApp.setNoiseControlModeInt(index); }
                            }

                            // Adaptive Noise Level Slider
                            Column {
                                visible: airPodsTrayApp.deviceInfo.adaptiveModeActive
                                width: parent.width
                                spacing: 6

                                RowLayout {
                                    width: parent.width
                                    Label {
                                        text: "Adaptive Noise Level"
                                        font.pixelSize: 12
                                        color: "#cbd5e1"
                                    }
                                    Item { Layout.fillWidth: true }
                                    Label {
                                        text: adaptiveSlider.value + "%"
                                        font.pixelSize: 12
                                        font.bold: true
                                        color: "#38bdf8"
                                    }
                                }

                                AppSlider {
                                    id: adaptiveSlider
                                    width: parent.width
                                    from: 0
                                    to: 100
                                    stepSize: 1
                                    value: airPodsTrayApp.deviceInfo.adaptiveNoiseLevel

                                    Timer {
                                        id: debounceTimer
                                        interval: 400
                                        onTriggered: if (!parent.pressed) airPodsTrayApp.setAdaptiveNoiseLevel(parent.value)
                                    }

                                    onPressedChanged: if (!pressed) airPodsTrayApp.setAdaptiveNoiseLevel(value)
                                    onValueChanged: if (pressed) debounceTimer.restart()
                                }
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // 3. Spatial Audio & Head Tracking Card
                    // ---------------------------------------------------------
                    Rectangle {
                        visible: airPodsTrayApp.airpodsConnected
                        width: parent.width - 32
                        anchors.horizontalCenter: parent.horizontalCenter
                        implicitHeight: spatialColumn.implicitHeight + 24
                        height: implicitHeight
                        radius: 12
                        color: "#161b26"
                        border.color: "#273146"
                        border.width: 1

                        Column {
                            id: spatialColumn
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 12
                            width: parent.width - 24
                            spacing: 12

                            RowLayout {
                                width: parent.width

                                Column {
                                    spacing: 2
                                    Label {
                                        text: "Spatial Head Tracking"
                                        font.pixelSize: 13
                                        font.bold: true
                                        color: "#f8fafc"
                                    }
                                    Label {
                                        text: "Real-time IMU sensor telemetry (UDP 4242 / OSC 9000)"
                                        font.pixelSize: 10
                                        color: "#64748b"
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                AppSwitch {
                                    id: spatialSwitch
                                    checked: airPodsTrayApp.deviceInfo.spatialTracker.enabled
                                    onCheckedChanged: airPodsTrayApp.deviceInfo.spatialTracker.enabled = checked
                                }
                            }

                            // Dynamic Visual Radar Area
                            Rectangle {
                                visible: spatialSwitch.checked
                                width: parent.width
                                implicitHeight: radarCol.implicitHeight + 20
                                height: implicitHeight
                                radius: 8
                                color: Qt.rgba(0.06, 0.08, 0.12, 0.8)
                                border.color: Qt.rgba(0.22, 0.65, 0.95, 0.4)
                                border.width: 1

                                Column {
                                    id: radarCol
                                    anchors.centerIn: parent
                                    width: parent.width - 16
                                    spacing: 8

                                    RowLayout {
                                        width: parent.width
                                        spacing: 8

                                        // Direction Compass Node
                                        Item {
                                            Layout.preferredWidth: 40
                                            Layout.preferredHeight: 40
                                            rotation: airPodsTrayApp.deviceInfo.spatialTracker.yaw

                                            Rectangle {
                                                anchors.centerIn: parent
                                                width: 32
                                                height: 32
                                                radius: 16
                                                color: Qt.rgba(0.14, 0.45, 0.85, 0.25)
                                                border.color: "#38bdf8"
                                                border.width: 2

                                                Rectangle {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    anchors.top: parent.top
                                                    anchors.topMargin: -3
                                                    width: 6
                                                    height: 6
                                                    radius: 3
                                                    color: "#38bdf8"
                                                }
                                            }
                                        }

                                        Column {
                                            Layout.fillWidth: true
                                            Layout.alignment: Qt.AlignVCenter
                                            spacing: 3

                                            Label {
                                                text: "Yaw: " + Math.round(airPodsTrayApp.deviceInfo.spatialTracker.yaw) + "°"
                                                font.pixelSize: 12
                                                font.bold: true
                                                color: "#f1f5f9"
                                            }
                                            Label {
                                                text: "P: " + Math.round(airPodsTrayApp.deviceInfo.spatialTracker.pitch) + "°  |  R: " + Math.round(airPodsTrayApp.deviceInfo.spatialTracker.roll) + "°"
                                                font.pixelSize: 10
                                                color: "#94a3b8"
                                            }
                                        }

                                        AppButton {
                                            Layout.alignment: Qt.AlignVCenter
                                            text: "Recenter"
                                            implicitHeight: 30
                                            onClicked: airPodsTrayApp.deviceInfo.spatialTracker.recenter()
                                        }
                                    }

                                    Label {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "Tip: Enable 'Windows Sonic for Headphones' in Windows Sound Settings"
                                        font.pixelSize: 9
                                        color: "#64748b"
                                    }
                                }
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // 4. Audio & Listening Features Card
                    // ---------------------------------------------------------
                    Rectangle {
                        visible: airPodsTrayApp.airpodsConnected
                        width: parent.width - 32
                        anchors.horizontalCenter: parent.horizontalCenter
                        implicitHeight: featuresColumn.implicitHeight + 24
                        height: implicitHeight
                        radius: 12
                        color: "#161b26"
                        border.color: "#273146"
                        border.width: 1

                        Column {
                            id: featuresColumn
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 12
                            width: parent.width - 24
                            spacing: 12

                            Label {
                                text: "LISTENING FEATURES"
                                font.pixelSize: 10
                                font.bold: true
                                color: "#60a5fa"
                            }

                            RowLayout {
                                width: parent.width

                                Column {
                                    spacing: 2
                                    Label {
                                        text: "Conversational Awareness"
                                        font.pixelSize: 13
                                        font.bold: true
                                        color: "#f8fafc"
                                    }
                                    Label {
                                        text: "Automatically reduces volume when you speak"
                                        font.pixelSize: 10
                                        color: "#64748b"
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                AppSwitch {
                                    checked: airPodsTrayApp.deviceInfo.conversationalAwareness
                                    onCheckedChanged: airPodsTrayApp.setConversationalAwareness(checked)
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: 1
                                color: "#222c3e"
                            }

                            RowLayout {
                                width: parent.width

                                Column {
                                    spacing: 2
                                    Label {
                                        text: "One Bud ANC Mode"
                                        font.pixelSize: 13
                                        font.bold: true
                                        color: "#f8fafc"
                                    }
                                    Label {
                                        text: "Keep ANC active when only one AirPod is in ear"
                                        font.pixelSize: 10
                                        color: "#64748b"
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                AppSwitch {
                                    checked: airPodsTrayApp.deviceInfo.oneBudANCMode
                                    onToggled: airPodsTrayApp.setOneBudANCMode(checked)
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: 1
                                color: "#222c3e"
                            }

                            RowLayout {
                                width: parent.width

                                Column {
                                    spacing: 2
                                    Label {
                                        text: "Hearing Aid Mode"
                                        font.pixelSize: 13
                                        font.bold: true
                                        color: "#f8fafc"
                                    }
                                    Label {
                                        text: "Enhances voice frequencies in your environment"
                                        font.pixelSize: 10
                                        color: "#64748b"
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                AppSwitch {
                                    checked: airPodsTrayApp.deviceInfo.hearingAidEnabled
                                    onCheckedChanged: airPodsTrayApp.setHearingAidEnabled(checked)
                                }
                            }
                        }
                    }

                    // Bottom padding spacer
                    Item {
                        width: parent.width
                        height: 10
                    }
                }
            }
        }
    }
    }

    // =========================================================================
    // SETTINGS PAGE
    // =========================================================================
    Component {
        id: settingsPage
        Page {
            id: settingsPageItem
            background: Rectangle { color: "#0f131c" }

            header: ToolBar {
                background: Rectangle {
                    color: "#161b26"
                    border.color: "#273146"
                    border.width: 1
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 12

                    RoundButton {
                        font.family: iconFont.name
                        font.pixelSize: 16
                        text: "\uecb1"
                        palette.buttonText: "#cbd5e1"
                        background: Rectangle {
                            radius: 14
                            color: parent.hovered ? "#222a3d" : "transparent"
                        }
                        onClicked: stackView.pop()
                    }

                    Label {
                        text: "Settings"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#f8fafc"
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            ScrollView {
                id: settingsScrollView
                anchors.fill: parent
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                contentWidth: availableWidth

                // Responsive auto-scaling container for settings
                Item {
                    id: settingsScaledWrapper
                    readonly property real baseWidth: 380
                    readonly property real uiScale: Math.min(1.0, settingsScrollView.availableWidth / baseWidth)

                    width: settingsScrollView.availableWidth
                    height: settingsContentCol.implicitHeight * uiScale

                    Column {
                        id: settingsContentCol
                        width: settingsScaledWrapper.baseWidth
                        scale: settingsScaledWrapper.uiScale
                        transformOrigin: Item.TopLeft
                        spacing: 14
                        padding: 16

                    // Ear Detection Card
                    Rectangle {
                        width: parent.width - 32
                        anchors.horizontalCenter: parent.horizontalCenter
                        implicitHeight: earCol.implicitHeight + 24
                        height: implicitHeight
                        radius: 12
                        color: "#161b26"
                        border.color: "#273146"
                        border.width: 1

                        Column {
                            id: earCol
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 12
                            width: parent.width - 24
                            spacing: 12

                            Label {
                                text: "EAR DETECTION"
                                font.pixelSize: 10
                                font.bold: true
                                color: "#60a5fa"
                            }

                            Label {
                                text: "Pause media when removing AirPods:"
                                font.pixelSize: 12
                                color: "#cbd5e1"
                            }

                            AppComboBox {
                                width: parent.width
                                model: ["One Removed", "Both Removed", "Never"]
                                currentIndex: airPodsTrayApp.earDetectionBehavior
                                onActivated: airPodsTrayApp.earDetectionBehavior = currentIndex
                            }

                            Rectangle {
                                visible: airPodsTrayApp.airpodsConnected
                                width: parent.width
                                height: 1
                                color: "#222c3e"
                            }

                            RowLayout {
                                visible: airPodsTrayApp.airpodsConnected
                                width: parent.width

                                Column {
                                    spacing: 2
                                    Label {
                                        text: "One Bud ANC Mode"
                                        font.pixelSize: 13
                                        font.bold: true
                                        color: "#f8fafc"
                                    }
                                    Label {
                                        text: "Keep ANC active when only one AirPod is in ear"
                                        font.pixelSize: 10
                                        color: "#64748b"
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                AppSwitch {
                                    checked: airPodsTrayApp.deviceInfo.oneBudANCMode
                                    onToggled: airPodsTrayApp.setOneBudANCMode(checked)
                                }
                            }
                        }
                    }

                    // System & Behavior Card
                    Rectangle {
                        width: parent.width - 32
                        anchors.horizontalCenter: parent.horizontalCenter
                        implicitHeight: sysCol.implicitHeight + 24
                        height: implicitHeight
                        radius: 12
                        color: "#161b26"
                        border.color: "#273146"
                        border.width: 1

                        Column {
                            id: sysCol
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 12
                            width: parent.width - 24
                            spacing: 12

                            Label {
                                text: "APP & SYSTEM"
                                font.pixelSize: 10
                                font.bold: true
                                color: "#60a5fa"
                            }

                            RowLayout {
                                width: parent.width
                                Label {
                                    text: "Auto-Start on Windows Login"
                                    font.pixelSize: 12
                                    color: "#cbd5e1"
                                }
                                Item { Layout.fillWidth: true }
                                AppSwitch {
                                    checked: airPodsTrayApp.autoStartManager.autoStartEnabled
                                    onCheckedChanged: airPodsTrayApp.autoStartManager.autoStartEnabled = checked
                                }
                            }

                            Rectangle { width: parent.width; height: 1; color: "#222c3e" }

                            RowLayout {
                                width: parent.width
                                Label {
                                    text: "System Notifications"
                                    font.pixelSize: 12
                                    color: "#cbd5e1"
                                }
                                Item { Layout.fillWidth: true }
                                AppSwitch {
                                    checked: airPodsTrayApp.notificationsEnabled
                                    onCheckedChanged: airPodsTrayApp.notificationsEnabled = checked
                                }
                            }

                            Rectangle { width: parent.width; height: 1; color: "#222c3e" }

                            RowLayout {
                                width: parent.width
                                Label {
                                    text: "Bluetooth Retry Attempts"
                                    font.pixelSize: 12
                                    color: "#cbd5e1"
                                }
                                Item { Layout.fillWidth: true }
                                AppSpinBox {
                                    from: 1
                                    to: 10
                                    value: airPodsTrayApp.retryAttempts
                                    onValueChanged: airPodsTrayApp.retryAttempts = value
                                }
                            }
                        }
                    }

                    // Device & Cloud Keys Card
                    Rectangle {
                        width: parent.width - 32
                        anchors.horizontalCenter: parent.horizontalCenter
                        implicitHeight: devCol.implicitHeight + 24
                        height: implicitHeight
                        radius: 12
                        color: "#161b26"
                        border.color: "#273146"
                        border.width: 1

                        Column {
                            id: devCol
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 12
                            width: parent.width - 24
                            spacing: 12

                            Label {
                                text: "ECOSYSTEM & ADVANCED"
                                font.pixelSize: 10
                                font.bold: true
                                color: "#60a5fa"
                            }

                            RowLayout {
                                width: parent.width
                                Label {
                                    text: "Cross-Device Connectivity"
                                    font.pixelSize: 12
                                    color: "#cbd5e1"
                                }
                                Item { Layout.fillWidth: true }
                                AppSwitch {
                                    checked: airPodsTrayApp.crossDeviceEnabled
                                    onCheckedChanged: airPodsTrayApp.setCrossDeviceEnabled(checked)
                                }
                            }

                            RowLayout {
                                visible: airPodsTrayApp.airpodsConnected
                                width: parent.width
                                spacing: 8

                                AppTextField {
                                    id: newNameField
                                    Layout.fillWidth: true
                                    placeholderText: airPodsTrayApp.deviceInfo.deviceName ? airPodsTrayApp.deviceInfo.deviceName : "AirPods Pro"
                                    maximumLength: 32
                                }

                                AppButton {
                                    text: "Rename"
                                    onClicked: airPodsTrayApp.renameAirPods(newNameField.text)
                                }
                            }

                            RowLayout {
                                visible: airPodsTrayApp.airpodsConnected
                                width: parent.width
                                spacing: 8

                                AppTextField {
                                    id: newPhoneMacField
                                    Layout.fillWidth: true
                                    placeholderText: (PHONE_MAC_ADDRESS !== "" ? PHONE_MAC_ADDRESS : "00:00:00:00:00:00")
                                    maximumLength: 32
                                }

                                AppButton {
                                    text: "Set MAC"
                                    onClicked: airPodsTrayApp.setPhoneMac(newPhoneMacField.text)
                                }
                            }

                            AppButton {
                                width: parent.width
                                text: "Show Magic Cloud Keys QR"
                                onClicked: keysQrDialog.show()
                            }

                            KeysQRDialog {
                                id: keysQrDialog
                                encKey: airPodsTrayApp.deviceInfo.magicAccEncKey
                                irk: airPodsTrayApp.deviceInfo.magicAccIRK
                            }
                        }
                    }

                    Item { width: parent.width; height: 16 }
                }
            }
        }
    }
}
}
