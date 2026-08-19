#include <sandbox/types.hpp>

#include <reflex/qt/moc/export.hpp>

REFLEX_QT_MODULE(reflex_sandbox_types, m)
{
  m.expose<^^sandbox>();
}

int main(int argc, char** argv)
{
  return reflex::qt::moc::export_main<reflex_sandbox_types>(argc, argv);
}
