import QtCore
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import Clock

ApplicationWindow {
    id: root
    visible: true
    width: 800
    height: 600
    title: qsTr("Clock")

    ClockController {
        id: controller
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        Label {
            text: controller.clockText
            font.pixelSize: 24
            font.family: "Consolas"
        }
    }
}
