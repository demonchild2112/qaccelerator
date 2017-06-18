// TODO(ogaro): Validate!
// TODO(ogaro): Handle locked tables gracefully. Ony one instance of the
// application should be allowed to exist. Enable creation of more than a single
// session (safely).
// TODO(ogaro): For each model, and for each field, have structs that store
// the most recent value (if that value is null, read from db; when writing to
// db, set that value too). Use these to keep track of the fields of the most
// recently read/written N rows.
// TODO(ogaro): All accessor methods should be const. Cast if necessary.
#ifndef QACCELERATOR_DB_H_
#define QACCELERATOR_DB_H_

#include "speed-grapher.h"
#include "qaccelerator-utils.h"
#include <unordered_map>
#include <iostream>
#include <string>
#include <QtSql/QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <memory>
#include <QString>
#include <QMap>
#include <vector>
#include <QDir>
#include <QDebug>


// Caller must make sure session does not go out of scope while models
// depending on it are still existent.
class Session {
 public:
  Session();

  bool Exec(const QString& query);
  QSqlQuery& GetQuery() {
    return *query_;
  }

 private:
  void CreateDownloadItemsTable();
  void CreatePreferencesTable();
  void CreateTable(const QString& table_name,
      const QMap<QString, QString>& types,
      const QMap<QString, QString>& extra_defs);

  QSqlDatabase db_;
  std::unique_ptr<QSqlQuery> query_;
};


template <typename T>
class Model {
 public:
  static const QString& TableName() {
    return table_name_;
  }

  static const QMap<QString, QString>& Types() {
    return types_;
  }

  static const QMap<QString, QString>& ExtraDefs() {
    return extra_defs_;
  }

  static Nullable<T> AddNew(
      const QMap<QString, QVariant>& data,
      Session* session) {
    QStringList fields, values;
    QMapIterator<QString, QVariant> it(data);
    while (it.hasNext()) {
      it.next();
      fields.append(it.key());
      values.append(Prepare(it.key(), it.value()));
    }
    QString query = QString("INSERT INTO %1 (%2) VALUES (%3)")
        .arg(table_name_)
        .arg(fields.join(","))
        .arg(values.join(","));
    session->Exec(query);
    QVariant id = session->GetQuery().lastInsertId();
    if (id.isValid()) {
      return T(session, id.toInt());
    } else {
      qDebug() << "Database INSERT operation did not return a row number.";
      return Nullable<T>();
    }
  }

  static Nullable<T> Get(Session* session, int id) {
    QString query = QString("SELECT id FROM %1 WHERE id = %2")
        .arg(TableName())
        .arg(id);
   session->Exec(query);
   if (session->GetQuery().next()) {
     return T(session, session->GetQuery().value(0).toInt());
   } else {
     return Nullable<T>();
   }
  }

  static Nullable<T> Get(Session* session,
                                    const QString& field,
                                    const QVariant& value) {
    QString query = QString("SELECT id FROM %1 WHERE %2 = %3")
        .arg(TableName())
        .arg(field)
        .arg(Prepare(field, value));
   session->Exec(query);
   if (session->GetQuery().next()) {
     int id = session->GetQuery().value(0).toInt();
     // Positions the query on the last record so that no next() call after this
     // returns results for this SELECT query.
     session->GetQuery().last();
     return T(session, id);
   } else {
     return Nullable<T>();
   }
  }

  static void GetAll(Session* session,
                     const QString& field,
                     const QVariant& value,
                     std::vector<T>* models) {
    QString query = QString("SELECT id FROM %1 WHERE %2 = %3")
        .arg(TableName())
        .arg(field)
        .arg(Prepare(field, value));
    session->Exec(query);
    while (session->GetQuery().next()) {
     int id = session->GetQuery().value(0).toInt();
     models->push_back(T(session, id));
    }
  }

  static void GetAll(Session* session, std::vector<T>* models) {
    QString query = QString("SELECT id FROM %1")
        .arg(TableName());
    session->Exec(query);
    while (session->GetQuery().next()) {
     int id = session->GetQuery().value(0).toInt();
     models->push_back(T(session, id));
    }
  }

  static int Count(Session* session) {
    QString query = QString("SELECT COUNT(*) FROM %1").arg(table_name_);
    session->Exec(query);
    if (session->GetQuery().next()) {
      return session->GetQuery().value(0).toInt();
    } else {
      return -1;
    }
  }

  bool Delete() {
    QString query = QString("DELETE FROM %1 WHERE id = %2")
        .arg(TableName())
        .arg(id_);
    return session_->Exec(query);
  }

  int Id() const {
    return id_;
  }

  bool IsValid() {
    return session_ != nullptr && id_ >= 0;
  }

protected:
  Model(Session* session, int id) : session_(session), id_(id) {}

  static const QString& GetType(const QString& field) {
    // TODO(ogaro): Validate!
    return types_.find(field).value();
  }

  static void GetFields(QStringList* fields) {
    QMap<QString, QString>::const_iterator it = types_.constBegin();
    while(it != types_.constEnd()) {
      fields->append(it.key());
    }
  }

  static QString Prepare(const QString& field, const QVariant& value) {
    if (GetType(field) == "VARCHAR") {
      return QString("'%1'").arg(value.toString());
    } else {
      return value.toString();
    }
  }

  Nullable<QVariant> GetField(const QString& field) {
    QString query = QString("SELECT %1 FROM %2 WHERE id = %4")
        .arg(field)
        .arg(TableName())
        .arg(id_);
   if (session_ == nullptr) {
     qDebug() << "Session is null.";
   }
   session_->Exec(query);
   if (session_->GetQuery().next()) {
     return session_->GetQuery().value(0);
   } else {
     return Nullable<QVariant>();
   }
  }

  bool SetField(const QString& field, const QVariant& val) {
    QString query = QString("UPDATE %1 SET %2 = %3 WHERE id = %4")
        .arg(TableName())
        .arg(field)
        .arg(Prepare(field, val))
        .arg(id_);
    return session_->Exec(query);
  }

  Session* session_;
  int id_;
  static const QString table_name_;
  static const QMap<QString, QString> types_;
  static const QMap<QString, QString> extra_defs_;
};

// TODO(ogaro): Category field is not needed.
// TODO(ogaro): Validate the field names!
class DownloadItem : public Model<DownloadItem> {
 public:
  enum class StatusEnum {
      IN_PROGRESS = 0,
      PAUSED = 1,
      COMPLETED = 2,
      CANCELLED = 3,
      FAILED = 4,
      QUEUED = 5
  };

  static StatusEnum MakeStatus(int enum_id) {
    switch(enum_id) {
    case 0:
      return StatusEnum::IN_PROGRESS;
    case 1:
      return StatusEnum::PAUSED;
    case 2:
      return StatusEnum::COMPLETED;
    case 3:
      return StatusEnum::CANCELLED;
    case 4:
      return StatusEnum::FAILED;
    case 5:
      return StatusEnum::QUEUED;
    default:
      std::cout << "Invalid status id: " << enum_id << std::endl;
      exit(-1);
    }
  }

  static int ToInt(const StatusEnum& status) {
    switch(status) {
    case StatusEnum::IN_PROGRESS:
      return 0;
    case StatusEnum::PAUSED:
      return 1;
    case StatusEnum::COMPLETED:
      return 2;
    case StatusEnum::CANCELLED:
      return 3;
    case StatusEnum::FAILED:
      return 4;
    case StatusEnum::QUEUED:
      return 5;
    default:
      // Control should never reach here!
      std::cout << "Unrecognized status." << std::endl;
      exit(-1);
    }
  }

  static QString ToString(const StatusEnum& status) {
    switch(status) {
    case StatusEnum::IN_PROGRESS:
      return "In Progress";
    case StatusEnum::PAUSED:
      return "Paused";
    case StatusEnum::COMPLETED:
      return "Completed";
    case StatusEnum::CANCELLED:
      return "Cancelled";
    case StatusEnum::FAILED:
      return "Failed";
    case StatusEnum::QUEUED:
      return "Queued";
    default:
      // Control should never reach here!
      std::cout << "Unrecognized status." << std::endl;
      exit(-1);
    }
  }

  DownloadItem(Session* session, int id)
      : Model<DownloadItem>::Model(session, id) {}

  DownloadItem() : Model<DownloadItem>::Model(nullptr, -1) {
      // TODO(ogaro): Should we be able to place these objects in containers?
  }

  // Getters

  Nullable<QString> Url() {
    Nullable<QVariant> value = GetField("url");
    if (value.IsNull()) {
      return Nullable<QString>();
    }
    return value.Get().toString();
  }

  Nullable<QString> SaveAs() {
    Nullable<QVariant> value = GetField("save_as");
    if (value.IsNull()) {
      return Nullable<QString>();
    }
    return value.Get().toString();
  }

  Nullable<double> Progress() {
    Nullable<QVariant> value = GetField("progress");
    if (value.IsNull()) {
      return Nullable<double>();
    }
    return value.Get().toDouble();
  }

  Nullable<qint64> StartTime() {
    Nullable<QVariant> value = GetField("start_time");
    if (value.IsNull()) {
      return Nullable<qint64>();
    }
    return value.Get().toLongLong();
  }

  Nullable<int> NumConnections() {
    Nullable<QVariant> value = GetField("num_connections");
    if (value.IsNull()) {
      return Nullable<int>();
    }
    return value.Get().toInt();
  }

  Nullable<int> ChunkSize() {
    Nullable<QVariant> value = GetField("chunk_size");
    if (value.IsNull()) {
      return Nullable<int>();
    }
    return value.Get().toInt();
  }

  Nullable<qint64> FileSize() {
    Nullable<QVariant> value = GetField("file_size");
    if (value.IsNull()) {
      return Nullable<qint64>();
    }
    return value.Get().toLongLong();
  }

  Nullable<QString> WorkDir() {
    Nullable<QVariant> value = GetField("work_dir");
    if (value.IsNull()) {
      return Nullable<QString>();
    }
    return value.Get().toString();
  }

  Nullable<StatusEnum> Status() {
    Nullable<QVariant> value = GetField("status");
    if (value.IsNull()) {
      return Nullable<StatusEnum>();
    }
    return MakeStatus(value.Get().toInt());
  }

  Nullable<qint64> MillisElapsed() {
    Nullable<QVariant> value = GetField("millis_elapsed");
    if (value.IsNull()) {
      return Nullable<qint64>();
    }
    return value.Get().toLongLong();
  }

  // Setters

  void SetUrl(const QString& url) {
    SetField("url", url);
  }

  void SetSaveAs(const QString& save_as) {
    SetField("save_as", save_as);
  }

  void SetProgress(double progress) {
    SetField("progress", progress);
  }

  void SetStartTime(qint64 start_time) {
    SetField("start_time", start_time);
  }

  void SetNumConnections(int num_connections) {
    SetField("num_connections", num_connections);
  }

  void SetChunkSize(int chunk_size) {
    SetField("chunk_size", chunk_size);
  }

  void SetFileSize(qint64 file_size) {
    SetField("file_size", file_size);
  }

  void SetWorkDir(const QString& work_dir) {
    SetField("work_dir", work_dir);
  }

  void SetStatus(const StatusEnum& status) {
    SetField("status", ToInt(status));
  }

  void SetMillisElapsed(qint64 millis_elapsed) {
    SetField("millis_elapsed", millis_elapsed);
  }
};


class Preference : public Model<Preference> {
 public:
  Preference(Session* session, int id)
    : Model<Preference>::Model(session, id) {}

  Nullable<QString> Name() {
    Nullable<QVariant> value = GetField("name");
    if (value.IsNull()) {
      return Nullable<QString>();
    }
    return value.Get().toString();
  }

  Nullable<QString> Value() {
    Nullable<QVariant> value = GetField("value");
    if (value.IsNull()) {
      return Nullable<QString>();
    }
    return value.Get().toString();
  }

  void SetName(const QString& name) {
    SetField("name", name);
  }

  void SetValue(const QString& value) {
    SetField("value", value);
  }
};

// Use mutexes in every public function.
class PreferenceManager {
 public:
  PreferenceManager(Session* session) : session_(session) {
    QString download_dir = QDir(QDir::homePath()).absoluteFilePath("Downloads");
    defaults_ = {
        {"in_progress_background_brush_color", "#fff"},
        {"in_progress_background_brush_alpha", 1.0},
        {"in_progress_grid_pen_color", "#ccc"},
        {"in_progress_grid_pen_alpha", 0.15},
        {"in_progress_grid_pen_width", 0.0},
        {"in_progress_curve_pen_color", "#060"},
        {"in_progress_curve_pen_alpha", 0.8},
        {"in_progress_curve_pen_width", 0.0},
        {"in_progress_curve_brush_color", "#0f0"},
        {"in_progress_curve_brush_alpha", 0.5},
        {"in_progress_chart_brush_color", "#0f0"},
        {"in_progress_chart_brush_alpha", 0.33},
        {"in_progress_indicator_dot_pen_color", "#ccc"},
        {"in_progress_indicator_dot_pen_alpha", 1.0},
        {"in_progress_indicator_dot_pen_width", 0.0},
        {"in_progress_indicator_dot_brush_color", "#0f0"},
        {"in_progress_indicator_dot_brush_alpha", 0.5},
        {"in_progress_indicator_dot_size", 8.0},
        {"in_progress_indicator_line_pen_color", "#ccc"},
        {"in_progress_indicator_line_pen_alpha", 1.0},
        {"in_progress_indicator_line_pen_width", 0.0},
        {"in_progress_indicator_text_font_name", "Verdana"},
        {"in_progress_indicator_text_font_size", 10.0},
        {"in_progress_indicator_text_color", "#ccc"},
        {"in_progress_indicator_text_alpha", 1.0},
        {"paused_background_brush_color", "#fff"},
        {"paused_background_brush_alpha", 1.0},
        {"paused_grid_pen_color", "#ccc"},
        {"paused_grid_pen_alpha", 0.15},
        {"paused_grid_pen_width", 0.0},
        {"paused_curve_pen_color", "#f06"},
        {"paused_curve_pen_alpha", 0.8},
        {"paused_curve_pen_width", 0.0},
        {"paused_curve_brush_color", "#f06"},
        {"paused_curve_brush_alpha", 0.5},
        {"paused_chart_brush_color", "#f06"},
        {"paused_chart_brush_alpha", 0.33},
        {"paused_indicator_dot_pen_color", "#ccc"},
        {"paused_indicator_dot_pen_alpha", 1.0},
        {"paused_indicator_dot_pen_width", 0.0},
        {"paused_indicator_dot_brush_color", "#f06"},
        {"paused_indicator_dot_brush_alpha", 0.5},
        {"paused_indicator_dot_size", 8.0},
        {"paused_indicator_line_pen_color", "#ccc"},
        {"paused_indicator_line_pen_alpha", 1.0},
        {"paused_indicator_line_pen_width", 0.0},
        {"paused_indicator_text_font_name", "Verdana"},
        {"paused_indicator_text_font_size", 10.0},
        {"paused_indicator_text_color", "#ccc"},
        {"paused_indicator_text_alpha", 1.0},
        {"completed_background_brush_color", "#fff"},
        {"completed_background_brush_alpha", 1.0},
        {"completed_grid_pen_color", "#ccc"},
        {"completed_grid_pen_alpha", 0.15},
        {"completed_grid_pen_width", 0.0},
        {"completed_curve_pen_color", "#777"},
        {"completed_curve_pen_alpha", 0.8},
        {"completed_curve_pen_width", 0.0},
        {"completed_curve_brush_color", "#777"},
        {"completed_curve_brush_alpha", 0.5},
        {"completed_chart_brush_color", "#777"},
        {"completed_chart_brush_alpha", 0.33},
        {"download_dir", download_dir},
        {"num_connections", 10},
        {"concurrent_cap", 2},
        {"multiple_filters", 0}
    };
    if (Preference::Count(session_) > 0) {
      return;
    }
    QMapIterator<QString, QVariant> it(defaults_);
    while (it.hasNext()) {
      it.next();
      Set(it.key(), it.value());
    }
  }

  void Set(const QString& preference, const QVariant& value) {
    EnsureValid(preference);
    Nullable<Preference> db_row = Preference::Get(
        session_,
        "name",
        preference);
    if (db_row.IsNull()) {
      Preference::AddNew({{"name", preference}, {"value", value}}, session_);
    } else {
      db_row.Get().SetValue(value.toString());
    }
  }

  void SetDefault(const QString& preference) {
    QVariant default_value = GetDefault(preference);
    Set(preference, default_value);
  }

  void GetDefault(const QString& preference, QString* read_value) {
   const QVariant* value = nullptr;
   GetDefaultOrDie(preference, &value);
   *read_value = value->toString();
  }

  void GetDefault(const QString& preference, double* read_value) {
   const QVariant* value = nullptr;
   GetDefaultOrDie(preference, &value);
   *read_value = value->toDouble();
  }

  void GetDefault(const QString& preference, int* read_value) {
   const QVariant* value = nullptr;
   GetDefaultOrDie(preference, &value);
   *read_value = value->toInt();
  }

  // TODO(ogaro): Repeating db_row.Get().Value() is not great style.
  // TODO(ogaro): Die in a more transparent manner (print message that says
  // preference "%s" does not exist.
  void Get(const QString& preference, QString* read_value) {
   EnsureValid(preference);
   Nullable<Preference> db_row = Preference::Get(session_,
                                                 "name",
                                                 preference);
   db_row.Get().Value();  // Will die if value is not in database.
   *read_value = db_row.Get().Value().Get();
  }

  void Get(const QString& preference, double* read_value) {
   EnsureValid(preference);
   Nullable<Preference> db_row = Preference::Get(session_,
                                                 "name",
                                                 preference);
   db_row.Get().Value();  // Will die if value is not in database.
   *read_value = QVariant(db_row.Get().Value().Get()).toDouble();
  }

  void Get(const QString& preference, int* read_value) {
   EnsureValid(preference);
   Nullable<Preference> db_row = Preference::Get(session_,
                                                 "name",
                                                 preference);
   db_row.Get().Value();  // Will die if value is not in database.
   *read_value = QVariant(db_row.Get().Value().Get()).toInt();
  }

  void Get(const QString& preference, bool* read_value) {
    QVariant value;
    Get(preference, &value);
    *read_value = value.toBool();
  }

  void Get(const QString& preference, QVariant* read_value) {
   EnsureValid(preference);
   Nullable<Preference> db_row = Preference::Get(session_,
                                                 "name",
                                                 preference);
   db_row.Get().Value();  // Will die if value is not in database.
   *read_value = QVariant(db_row.Get().Value().Get());
  }

  void ApplySavedPreferences(SpeedGrapherState state,
                             SpeedGrapher* speed_grapher,
                             bool show_indicators) {
    QString str_state = ToString(state);
    QMapIterator<QString, QVariant> it(defaults_);
    while (it.hasNext()) {
      it.next();
      if (it.key().startsWith(str_state)) {
        QString key(it.key());
        key = key.replace(str_state, "");
        key = key.mid(1);
        QVariant value;
        Get(it.key(), &value);
        speed_grapher->SetStyleAttribute(key, value);
      }
    }
    speed_grapher->UpdatePlot(show_indicators);
  }

  // TODO(ogaro): This is a lazy way of getting the Session in the download
  // manager.
  Session* GetSession() {
    return session_;
  }

 private:
  const QVariant& GetDefault(const QString& preference) {
    const QVariant* value;
    GetDefaultOrDie(preference, &value);
    return *value;
  }

 void GetDefaultOrDie(const QString& preference, const QVariant** read_value) {
   auto it = defaults_.find(preference);
   if (it != defaults_.end()) {
     *read_value = &(it.value());
   } else {
     qDebug() << "Preference '" << preference << "' not recognized.";
     exit(-1);
   }
 }

 void EnsureValid(const QString& preference) {
   const QVariant* value;
   GetDefaultOrDie(preference, &value);
 }

 QMap<QString, QVariant> defaults_;
 Session* session_;
};

#endif // QACCELERATOR_DB_H_
