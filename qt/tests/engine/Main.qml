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

    property int held: 0

    property IntHolder holder: IntHolder {
        id: holder

        value: 20

        onDoubled: value => root.held = value
    }

    Component.onCompleted: {
        probe.ping()
        holder.twice()
    }
}
