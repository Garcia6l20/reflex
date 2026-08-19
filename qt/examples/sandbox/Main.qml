import QtQml
import Reflex.Sandbox

QtObject {
    id: root

    property Sandbox sandbox: Sandbox {
        id: box

        onReported: text => box.observe(text)
    }

    property int width: box.range.high - box.range.low

    onWidthChanged: box.observe(box.caption())

    Component.onCompleted: {
        box.range = box.widen(box.range, 5)
        box.shade = Palette.Shade.dark
        box.report()
        box.observe(Settings.title)
        Qt.exit(0)
    }
}
