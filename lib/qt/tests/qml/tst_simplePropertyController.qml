import QtQuick
import QtTest

TestCase {
    name: "SimplePropertyrTests"

    readonly property QtObject controller: simplePropertyController

    // NOTE:
    //  1. cannot use aliased property since it shall refer to an object in current scope
    //  2. properties are one-way... if we set ivalue, it removes its binding to the controller
    readonly property int ivalue: controller.ivalue

    function test_base() {
      controller.ivalue = 0;
      compare(controller.ivalue, 0);
      controller.ivalue++;
      compare(controller.ivalue, 1);
    }

    function test_incr() {
      controller.ivalue = 0;
      controller.incr();
      compare(controller.ivalue, 1);
    }

    function test_binding() {
      controller.ivalue = 42;
      compare(ivalue, 42, "bounded property has not been updated");
      controller.incr();
      compare(ivalue, 43, "bounded property has not been updated");
      controller.ivalue++;
      compare(ivalue, 44, "bounded property has not been updated");
    }
}
