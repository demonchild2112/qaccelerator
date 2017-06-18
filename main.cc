#include "downloads-table.h"
#include "speed-grapher.h"
#include "download-monitor.h"
#include "qaccelerator.h"
#include "qaccelerator-utils.h"
#include "download-dialog.h"
#include "preferences-dialog.h"
#include <QApplication>

int main(int argc, char *argv[]) {
  // InitApplication();
  QApplication a(argc, argv);
  MainWindow m;
  int ret_value = a.exec();
  // fclose(stderr);
  return ret_value;
}
