#include "moc-pair.hpp"

#include <reflex/qt/moc/export.hpp>

REFLEX_QT_MODULE(pair_types, m)
{
  m.expose<twin>();
  m.expose<twin_qml>();
  m.expose<twin_gadget>();
  m.expose<twin_base>();
  m.expose<twin_derived>();
  m.expose<twin_flags>();
  m.expose<twin_styled>();
}

int main(int argc, char** argv) { return reflex::qt::moc::export_main<pair_types>(argc, argv); }
