# reflex.qt

Reflection-based Qt for c++26.

## Motivation

Qt may be very verbose an comes with a lot of boilerplate, especially when binding Qml with C++ core controller.
This library aims to fix this by using reflection on QObject derived classes.

## Features

- ✔️ **Signals** using a simple `signal<...>` member:

```cpp
class int_provider : qt::object<int_provider> {
  signal<int> sig;
};
```

- ✔️ **Slots** using `slot` annotation:

```cpp
class int_consumer : qt::object<int_provider> {
  [[=slot]] void onData(int value) { std::println("got {}", value);  }
};
```

- ✔️ **Connections** remains unchanged:

```cpp
QObject::connect(int_provider_obj, &int_provider::sig, int_consumer_obj, &int_consumer::onData);
```

- ✔️ **Properties** using `prop` annotated member function.
  - ✔️ *(optional)* **Listeners** using `listener_of<^^myProp>` annotation.
  - ✔️ *(optional)* **Setters** using `setter_of<^^myProp>]]` annotation.
  - ✔️ *(optional)* **Getters** using `getter_of<^^myProp>]]` annotation.

```cpp
class data : qt::object<data> {
  // property with read-write-notify flags (fully QML bindable)
  [[=prop<"rwn">]] int p1;

  // optional getter
  [[=getter_of<^^p1>]] int getP1() {
    return p1 * 2;
  }

  // optional setter
  [[=setter_of<^^p1>]] void setP1(int value) {
    p1 = value / 2;
  }

  // optional listener
  [[=listener_of<^^p1>]] void p1Changed() {
    std::println("p1 = {}", p1);
  }

};

int main(void) {
  data d;
  // writing
  d.setProperty("p1", 42); // OK - regular Qt invocation
  d.setProperty<"p1">(42); // OK - faster
  d.setProperty<^^data::p1>(42); // OK - faster (but needs public visibility)
  // reading
  d.property("p1"); // OK - regular Qt invocation, returns a QVariant
  d.property<"p1">(); // OK - faster, returns an int
  d.property<^^data::p1>(); // OK - faster, returns an int (but needs public visibility)
}
```

- ✔️ Timer event: using a simple `timer_event` annotation:

```cpp
class obj : qt::object<obj> {

  obj(QObject *parent = nullptr) : qt::object<obj>{parent} {
    startTimer<^^myTimerEvent>(1000);
  }

  // optional getter
  [[=timer_event]] int myTimerEvent() {
    std::println("event occurred at: {}", std::chrono::system_clock::now());
    // killTimer<^^myTimerEvent>(); // stops the timer
  }
};
```

- ❌ Custom types: not handled yet.
- ❌ Enums: not handled yet.
- ❓ Missing something ???
