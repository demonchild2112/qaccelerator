#ifndef CATEGORIZER_H_
#define CATEGORIZER_H_

#include <QSet>
#include "qaccelerator-utils.h"

class Categorizer {
 public:
  static Category Categorize(const QString& file_ext);

 private:
  Categorizer() {}

  static const QSet<QString> videos_;
  static const QSet<QString> images_;
  static const QSet<QString> audio_;
  static const QSet<QString> documents_;
  static const QSet<QString> software_;
};

#endif // CATEGORIZER_H_
