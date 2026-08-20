pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import Reflex.Clock

Window {
    id: root

    property Clock clock: Clock {
        id: ticker

        onFinished: banner.text = "finished"
        onReported: text => banner.text = text
    }

    readonly property color ink: ticker.shade === Palette.Shade.dark ? "#f2f2f2" : "#101010"

    width: 520
    height: 260
    visible: true
    title: Settings.title
    color: ticker.shade === Palette.Shade.light
           ? "#fbfbfb"
           : ticker.shade === Palette.Shade.mid ? "#9aa7b4" : "#20262d"

    component Button: Rectangle {
        property alias text: caption.text

        signal clicked

        width: 84
        height: 32
        radius: 4
        color: hover.containsMouse ? Qt.lighter(root.color, 1.25) : Qt.darker(root.color, 1.15)
        border.color: root.ink
        border.width: 1

        Text {
            id: caption

            anchors.centerIn: parent
            color: root.ink
        }

        MouseArea {
            id: hover

            anchors.fill: parent
            hoverEnabled: true
            onClicked: parent.clicked()
        }
    }

    Component.onCompleted: ticker.start(1000)

    Column {
        anchors.centerIn: parent
        spacing: 14

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.ink
            font.pixelSize: 40
            text: ticker.label === "" ? "ready" : ticker.label
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: root.ink
            text: "tick " + ticker.ticks + " of " + ticker.run.high
                  + ", run width " + ticker.run.width()
        }

        Text {
            id: banner

            anchors.horizontalCenter: parent.horizontalCenter
            color: root.ink
            text: "no report yet"
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            Button {
                text: "start"
                onClicked: {
                    ticker.rewind()
                    ticker.start(1000)
                }
            }

            Button {
                text: "stop"
                onClicked: ticker.stop()
            }

            Button {
                text: "widen"
                onClicked: ticker.run = ticker.widen(ticker.run, 2)
            }

            Button {
                text: "shade"
                onClicked: ticker.shade = (ticker.shade + 1) % 3
            }

            Button {
                text: "report"
                onClicked: ticker.report()
            }
        }
    }
}
