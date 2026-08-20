import QtQml
import Reflex.Clock

QtObject {
    id: root

    property Clock clock: Clock {
        id: ticker

        onFinished: Qt.exit(0)
        onReported: text => ticker.observe(text)
    }

    property string caption: ticker.label

    onCaptionChanged: ticker.observe(Settings.title)

    Component.onCompleted: {
        ticker.run = ticker.widen(ticker.run, 5)
        ticker.shade = Palette.Shade.dark
        ticker.report()
        ticker.start(1)
    }
}
