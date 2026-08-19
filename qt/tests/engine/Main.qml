import QtQml
import Reflex.EngineTest

QtObject {
    id: root

    property int heard: 0

    property Probe probe: Probe {
        id: probe

        level: 41

        onPinged: value => root.heard = value
    }

    Component.onCompleted: probe.ping()
}
