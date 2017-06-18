#include "qaccelerator-db.h"

using std::string;
using std::pair;

template<> const QString Model<DownloadItem>::table_name_ = "download_items";
template<> const QString Model<Preference>::table_name_ = "preferences";

// TODO(ogaro): Chunk size is not needed.
template<> const QMap<QString, QString> Model<DownloadItem>::types_ = {
    {"id", "INTEGER"},
    {"url", "VARCHAR"},
    {"save_as", "VARCHAR"},
    {"progress", "REAL"},
    {"file_size", "INTEGER"},
    {"start_time", "INTEGER"},
    {"num_connections", "INTEGER"},
    {"chunk_size", "INTEGER"},
    {"work_dir", "VARCHAR"},
    {"status", "INTEGER"},
    {"millis_elapsed", "INTEGER"}
};

template<> const QMap<QString, QString> Model<DownloadItem>::extra_defs_ = {
    {"id", "PRIMARY KEY"},
    {"url",  ""},
    {"save_as", "VARCHAR"},
    {"progress", "DEFAULT 0"},
    {"file_size", ""},
    {"start_time",  ""},
    {"num_connections",  ""},
    {"chunk_size", ""},
    {"work_dir", ""},
    {"status",  "DEFAULT 0"},
    {"millis_elapsed", "DEFAULT 0"}
};

template<> const QMap<QString, QString> Model<Preference>::types_ = {
    {"id", "INTEGER"},
    {"name", "VARCHAR"},
    {"value", "VARCHAR"}
};

template<> const QMap<QString, QString> Model<Preference>::extra_defs_ = {
    {"id",  "PRIMARY KEY"},
    {"name", ""},
    {"value", ""}
};

// List of sqlite pragmas to be applied to the db.
// Has to be of size at least one. Ok I need to replace this with something
// less bizarre.
static const char* kDbPragma[] = {
    "synchronous=OFF",
    "count_changes=OFF",
    nullptr
};

Session::Session() {
  db_ = QSqlDatabase::addDatabase("QSQLITE");
  db_.setDatabaseName("qaccelerator.db");
  if (!db_.open()) {
    DIE() << "Error while trying to open database: "
          << db_.lastError().text();
  }
  query_.reset(new QSqlQuery(db_));

  // Enable SQLite pragmas
  int i = 0;
  do {
    QString sql = QString("PRAGMA %1").arg(kDbPragma[i]);
    Exec(sql);
  } while (kDbPragma[++i] != nullptr);

  CreateDownloadItemsTable();
  CreatePreferencesTable();
}

void Session::CreateTable(
    const QString& table_name,
    const QMap<QString, QString>& types,
    const QMap<QString, QString>& extra_defs) {

  QString query = QString("CREATE TABLE IF NOT EXISTS %1 (")
     .arg(table_name);
  QStringList col_defs;
  QMapIterator<QString, QString> it(types);
  while (it.hasNext()) {
    it.next();
    QString col_name = it.key();
    QString col_type = it.value();
    QString extra_def = extra_defs[col_name];
    col_defs.append(
          QString("%1 %2 %3").arg(col_name).arg(col_type).arg(extra_def));
  }
  query += col_defs.join(",");
  query += ")";

  // Execute built query.
  Exec(query);
}

void Session::CreateDownloadItemsTable() {
  CreateTable(DownloadItem::TableName(), DownloadItem::Types(),
              DownloadItem::ExtraDefs());
}

void Session::CreatePreferencesTable() {
  CreateTable(Preference::TableName(), Preference::Types(),
              Preference::ExtraDefs());
}

bool Session::Exec(const QString& query) {
  if (query_->exec(query)) {
    return true;
  } else {
    std::string last_query = query_->lastQuery().toStdString();
    std::string error = query_->lastError().text().toStdString();
    std::cout << "Error while executing \"" << last_query << "\": "
              << error << std::endl;
    return false;
  }
}
