#include <doctest/doctest.h>

#include <reflex/qt.hpp>

#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <string_view>
#include <vector>

namespace qt = reflex::qt;

template <typename T> struct stack : qt::object<stack<T>>
{
  using base                               = qt::object<stack<T>>;
  template <typename... Args> using signal = typename base::template signal<Args...>;

  friend qt::access<stack<T>>;

  signal<T> pushed{this};

  [[= qt::slot]] void push(T v)
  {
    items.push_back(v);
    this->template setProperty<"depth">(int(items.size()));
    pushed(v);
  }

  [[= qt::invocable]] T at(int i) const
  {
    return items[std::size_t(i)];
  }

  [[= qt::prop{}]] int depth = 0;

private:
  [[= qt::prop{}]] T seed{};

  std::vector<T> items;
};

template <typename T, int N> struct fixed : qt::gadget<fixed<T, N>>
{
  [[= qt::prop{}]] T value{};

  [[= qt::invocable]] int capacity() const
  {
    return N;
  }
};

struct int_stack : qt::object<int_stack, stack<int>>
{
  [[= qt::slot]] void clear()
  {
    this->template setProperty<"depth">(0);
    ++clears;
  }

  [[= qt::prop{}]] int clears = 0;
};

TEST_CASE("a class template publishes one meta object per instantiation")
{
  CHECK(std::string_view{stack<int>::staticMetaObject.className()} == "stack<int>");
  CHECK(std::string_view{stack<QString>::staticMetaObject.className()} == "stack<QString>");
  CHECK(&stack<int>::staticMetaObject != &stack<QString>::staticMetaObject);

  stack<int> s;
  CHECK(s.metaObject() == &stack<int>::staticMetaObject);
  CHECK(std::string_view{s.metaObject()->superClass()->className()} == "QObject");
}

TEST_CASE("signals, slots and properties work on a template instantiation")
{
  stack<int> s;

  int pushed = 0;
  QObject::connect(&s, &stack<int>::pushed, [&](int v) { pushed = v; });

  int depth_changes = 0;
  QObject::connect(&s, &stack<int>::propertyChanged<"depth">, [&] { ++depth_changes; });

  s.push(7);
  CHECK(pushed == 7);
  CHECK(depth_changes == 1);
  CHECK(s.depth == 1);
  CHECK(s.at(0) == 7);
}

TEST_CASE("the dynamic meta object reaches a template's members by name")
{
  stack<QString> s;
  const auto*    mo = s.metaObject();

  REQUIRE(mo->indexOfMethod("push(QString)") >= 0);
  REQUIRE(QMetaObject::invokeMethod(&s, "push", Q_ARG(QString, QStringLiteral("a"))));
  CHECK(s.depth == 1);

  const auto depth = mo->property(mo->indexOfProperty("depth"));
  CHECK(depth.read(&s).toInt() == 1);

  const auto seed = mo->property(mo->indexOfProperty("seed"));
  REQUIRE(seed.isValid());
  REQUIRE(seed.write(&s, QStringLiteral("z")));
  CHECK(seed.read(&s).toString() == "z");

  QString    out;
  const auto at = mo->method(mo->indexOfMethod("at(int)"));
  REQUIRE(at.invoke(&s, qReturnArg(out), 0));
  CHECK(out == "a");
}

TEST_CASE("two instantiations are unrelated types to Qt")
{
  stack<int>     si;
  stack<QString> ss;

  CHECK(qobject_cast<stack<int>*>(static_cast<QObject*>(&ss)) == nullptr);
  CHECK(qobject_cast<stack<int>*>(static_cast<QObject*>(&si)) == &si);
  CHECK(not si.metaObject()->inherits(&stack<QString>::staticMetaObject));
}

TEST_CASE("a gadget template takes non-type parameters")
{
  const auto& mo = fixed<double, 4>::staticMetaObject;
  CHECK(std::string_view{mo.className()} == "fixed<double, 4>");
  CHECK(std::string_view{QMetaType::fromType<fixed<double, 4>>().name()} == "fixed<double,4>");
  CHECK(&mo != &fixed<double, 8>::staticMetaObject);

  fixed<double, 4> f{.value = 1.5};
  int              capacity = 0;
  REQUIRE(mo.method(mo.indexOfMethod("capacity()")).invokeOnGadget(&f, qReturnArg(capacity)));
  CHECK(capacity == 4);

  const auto value = mo.property(mo.indexOfProperty("value"));
  REQUIRE(value.writeOnGadget(&f, 2.5));
  CHECK(f.value == 2.5);
}

TEST_CASE("a plain class inherits a template instantiation")
{
  int_stack d;

  CHECK(std::string_view{d.metaObject()->className()} == "int_stack");
  CHECK(std::string_view{d.metaObject()->superClass()->className()} == "stack<int>");
  CHECK(d.metaObject()->inherits(&stack<int>::staticMetaObject));
  CHECK(qobject_cast<stack<int>*>(static_cast<QObject*>(&d)) == &d);

  int pushed = 0;
  QObject::connect(&d, &stack<int>::pushed, [&](int v) { pushed = v; });
  d.push(3);
  CHECK(pushed == 3);
  CHECK(d.depth == 1);

  REQUIRE(QMetaObject::invokeMethod(&d, "clear"));
  CHECK(d.depth == 0);
  CHECK(d.clears == 1);
}
