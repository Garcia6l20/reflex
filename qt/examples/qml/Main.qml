import QtQml
import Reflex.Demo

QtObject {
    property Counter counter: Counter {
        onValueChanged: {
            console.log(counter.caption())
            if (counter.value >= 3)
                Qt.exit(0)
        }
    }

    Component.onCompleted: {
        counter.bump(1)
        counter.bump(2)
    }
}
