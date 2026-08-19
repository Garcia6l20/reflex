import QtQml
import Reflex.Clock

QtObject {
    id: root

    property Clock clock: Clock {
        id: ticker

        onFinished: Qt.exit(0)
    }

    property string caption: ticker.label

    onCaptionChanged: ticker.observe()

    Component.onCompleted: ticker.start(20)
}
