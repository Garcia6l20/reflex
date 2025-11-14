import QtCore
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import Example1

ApplicationWindow {
    id: root
    visible: true
    width: 800
    height: 600
    title: qsTr("Example1")

    Example {
        id: example
        running: running.checked
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        Button {
            id: running
            text: checked ? "Pause" : "Run"
            checkable: true
            checked: true
        }
        Label {
            text: example.clockText
            font.pixelSize: 24
            font.family: "Consolas"
        }
        Button {
            text: "Trigger Sig1"
            onClicked: example.intSig(55, 55)
        }
        SpinBox {
            from: 0
            to: 100
            value: 42
            onValueChanged: example.slot1(value)
        }
        Button {
            text: "Say the truth"
            checkable: true
            onClicked: {
                console.log("was lying:", example.sayTheTruth(checked));
            }
        }
    }

    property message lastMsg

    Text {
        id: label
        color: "black"
        wrapMode: Text.WordWrap
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.topMargin: root.SafeArea.margins.top + 20
        anchors.leftMargin: root.SafeArea.margins.left + 20
        anchors.rightMargin: root.SafeArea.margins.right + 20
        anchors.bottomMargin: root.SafeArea.margins.bottom + 20

        Connections {
            target: example
            function onSend(msg) {
                console.debug(msg);
                label.text = msg.subject + "\n" + msg.body;
                root.lastMsg = msg;
            }
        }
    }

    Connections {
        target: example
        function onIntSig(value1, value2) {
            console.debug("sig1: ", value1, value2);
        }
    }
}
