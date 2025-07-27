import QtQuick
import QtTest

TestCase {
    id: root
    name: "SignalSlotsTests"

    readonly property QtObject controller: signalSlotController
    property bool value: true
    property int intValue1: -1
    property int intValue2: -1

    signal intSig1(a: int, b: int)
    signal intSig2(a: int, b: int)

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

    function test_signal_complete_signature() {
        // console.log(JSON.stringify(controller));

        // FIXME: this does not work... only _a is received correctly
        //        while it is received as expected by the global slot (in Connections block)
        // var a = false;
        // var b = false;
        // const receiver = function(_a: int, _b: int) {
        //     a = _a;
        //     b = _b;
        //     console.log("received: ", a, b);
        // };
        // controller.sig1.connect(receiver);
        console.log("controller.sig1(42)");
        controller.sig1(42);
        // compare(a, 42);
        // compare(b, undefined);
        compare(intValue1, 42);
        compare(intValue2, -1);
        console.log("controller.sig1(1,2)");
        controller.sig1(1, 2);
        // compare(a, 1);
        // compare(b, 2); // TODO: we have our-defaulted value, we should have the signal one
        compare(intValue1, 1);
        compare(intValue2, 2);
    }

    function test_slot_received() {
        intSig1(1, 2);
        compare(controller.slt1Value1, 1);
        compare(controller.slt1Value2, 2);
        intSig2(11, 22);
        compare(controller.slt1Value1, 1);
        compare(controller.slt1Value2, 2);
        compare(controller.slt2Value1, 11);
        compare(controller.slt2Value2, 22);
    }

    Connections {
        target: controller
        function onToggled(v) {
            root.value = v;
        }
        function onSig1(a, b) {
            console.log("received: ", a, b);
            root.intValue1 = a;
            root.intValue2 = b;
        }
    }
    Connections {
        target: root
        function onIntSig1(a, b) {
            controller.slt1(a, b);
        }
        function onIntSig2(a, b) {
            controller.slt2(a, b);
        }
    }
}
