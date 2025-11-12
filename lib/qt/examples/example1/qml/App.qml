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
    title: qsTr("Clock")

    Example {
        id: example
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        Label {
            text: example.clockText
            font.pixelSize: 24
            font.family: "Consolas"
        }
        Button {
            text: "Trigger Sig1"
            onClicked: example.intSig(55)
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

    Connections {
        target: example
        function onIntSig(value) {
            console.debug("sig1 value: ", value);
        }
    }
}
