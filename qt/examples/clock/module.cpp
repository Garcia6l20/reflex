#include <clock/types.hpp>

#include <reflex/qt/moc/export.hpp>

REFLEX_QT_MODULE(reflex_clock_types, m)
{
  m.expose<^^clock_example>();
}

int main(int argc, char** argv)
{
  return reflex::qt::moc::export_main<reflex_clock_types>(argc, argv);
}
