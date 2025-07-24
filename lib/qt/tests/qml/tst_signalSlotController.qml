import QtQuick
import QtTest

TestCase {
    name: "GetSetPropertyTests"

    readonly property QtObject controller: getSetPropertyController

    function test_base() {
        controller.ivalue = 1;
        compare(controller.ivalue, 4);
        controller.ivalue++; // read as 4, written with 5 => next read is 20
        compare(controller.ivalue, 20);
    }
}
