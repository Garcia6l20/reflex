#include <engine/types.hpp>

#include <reflex/qt/moc/export.hpp>

REFLEX_QT_MODULE(reflex_engine_test_types, m)
{
  m.expose<^^engine_test>();
}

int main(int argc, char** argv)
{
  return reflex::qt::moc::export_main<reflex_engine_test_types>(argc, argv);
}
