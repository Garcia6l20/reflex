import QtQuick
import QtTest

TestCase {
    name: "InvocableTests"

    readonly property QtObject controller: invocableTestController

    function test_mult() {
        compare(controller.mult(2, 2), 4, "oops");
    }
}
