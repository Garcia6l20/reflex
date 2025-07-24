import QtQuick
import QtTest

TestCase {
    id: root
    name: "SignalSlotsTests"

    readonly property QtObject controller: signalSlotController
    property bool value: true

    function test_base() {
        compare(value, true);
        controller.trig();
        compare(value, false);
        controller.trig();
        compare(value, true);
        controller.trig();
        compare(value, false);
        controller.trig();
        compare(value, true);
    }

    Connections {
        target: controller
        function onToggled(v) {
            root.value = v;
        }
    }
}
