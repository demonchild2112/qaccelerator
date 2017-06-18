// TODO(ogaro): Fetch actual downloaded bytes after download signal received
// (esp for unknown file sizes). Add a dummy data point for uknown file size
// downloads.

#ifndef QACCELERATOR_UTILS_H_
#define QACCELERATOR_UTILS_H_

#include <iostream>
#include <QDebug>
#include <unordered_map>
#include <QDir>
#include <QFileInfo>
#include <math.h>
#include <QDateTime>
#include <memory>
#include <QMetaType>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>

typedef std::pair<qint64, qint64> Segment;

struct DieOnDestroyed {
  ~DieOnDestroyed () {
    exit(-1);
  }
};

// TODO(ogaro): Display error message then quit.
#define DIE() (DieOnDestroyed(), qDebug() << "FATAL: ")

#define CHECK(condition)\
  if(!condition) {\
    DIE() << "Check failed: " << QString(#condition);\
  }\

static const int kMaxConnections = 1000;
static const char* kLaunchFName = "LAUNCH";
static const char* kDbFName = "qaccelerator.db";

// Time to wait for a new download to initiate before starting another.
static const int kDownloadWaitTimeMs = 3000;
static const qint64 kProgressUpdateInterval = 200;

static const char* kUmemeStyle =
    "QGroupBox { "
    "  border: 0; "
    "}"
    "QGraphicsView { "
    "  border: 1px solid #ddd; "
    "}";

namespace std {
// This allows us to use QStrings as keys in STL containers.
template<> struct hash<QString> {
  size_t operator()(const QString& str) const {
    return qHash(str);
  }
};
}

enum class SpeedGrapherState {
  IN_PROGRESS = 0,
  PAUSED = 1,
  COMPLETED = 2
};

enum class Category {
  VIDEOS = 0,
  IMAGES = 1,
  AUDIO = 2,
  DOCUMENTS = 3,
  SOFTWARE = 4,
  OTHER = 5
};

QString CategoryToString(const Category& category);

Category StringToCategory(const QString& category);

// Requirements for type T: Must be copyable (must have a copy constructor).
template<typename T>
class Nullable {
 public:
  // Non-explicit on purpose. We want implicit conversion of T values to
  // Nullable<T>
  Nullable(const T& val) {
    val_.reset(new T(val));
  }
  // Default constructor creates a null Nullable.
  Nullable() {}
  // Compiler needs us to specify explicit copy constructor
  Nullable(const Nullable<T>& other) {
    if (!other.IsNull()) {
      val_.reset(new T(other.Get()));
    }
  }

  Nullable<T>& operator=(const Nullable<T>& other) {
    if (!other.IsNull()) {
      val_.reset(new T(other.Get()));
    }
    return *this;
  }

  // TODO(ogaro): Return a const reference.
  T& Get () const {
    if (val_ != nullptr) {
      return *val_;
    }
    DIE() << "Nullable Error: Attempted to retrieve non-existent value.";
    return *val_;
  }

  void Set(const T& val) {
    val_.reset(new T(val));
  }

  bool IsNull () const {
    return val_ == nullptr;
  }

 private:
  std::unique_ptr<T> val_;
};


struct DownloadParams {
 public:
  DownloadParams(const QString& url, const QString& save_as, qint64 file_size,
                 int num_connections)
    : url_(url),
      save_as_(save_as),
      file_size_(file_size),
      num_connections_(num_connections) {}

  DownloadParams() {
    DownloadParams("", "", 0, 0);
  }
  ~DownloadParams() {}

  const QString& Url() const { return url_; }
  const QString& SaveAs() const { return save_as_; }
  qint64 FileSize() const { return file_size_; }
  int NumConnections() const { return num_connections_; }
  bool Accelerable() const { return accelerable_; }

  void SetUrl(const QString& url) { url_ = url; }
  void SetSaveAs(const QString& save_as) { save_as_ = save_as; }
  void SetFileSize(qint64 file_size) { file_size_ = file_size; }
  void SetNumConnections(int num_connections) {
    num_connections_ = num_connections;
  }
  void SetAccelerable(bool accelerable) {
    accelerable_ = accelerable;
  }

 private:
  QString url_;  // TODO(ogaro): Use QUrl?
  QString save_as_;  // Ditto.
  qint64 file_size_;
  int num_connections_;
  bool accelerable_;
};
Q_DECLARE_METATYPE(DownloadParams)

class StopWatch {
 public:
  StopWatch();

  void Start();
  void Stop();
  void Reset();
  void Reset(qint64 time_before_start);
  qint64 GetTimeElapsed();
  QString GetTimeElapsedString();

 private:
  qint64 start_time_;
  bool is_running_;
  qint64 time_before_start_;
};

struct FileSpec {
 public:
  FileSpec(int id, const QString& url) : id_(id), url_(url) {}
  FileSpec(const FileSpec& other) {
    id_ = other.Id();
    url_ = other.Url();
    mime_type_ = other.MimeType();
    file_size_ = other.FileSize();
    accelerable_ = other.Accelerable();
  }
  FileSpec() {}

  int Id() const { return id_; }
  QString Url() const { return url_; }
  bool Accelerable() const { return accelerable_; }
  const Nullable<QString>& MimeType() const { return mime_type_; }
  const Nullable<qint64>& FileSize() const { return file_size_; }

  void SetAccelerable(bool accelerable) { accelerable_ = accelerable; }
  void SetMimeType(const QString& mime_type) {
    mime_type_.Set(mime_type);
  }
  void SetFileSize(qint64 file_size) {
    file_size_.Set(file_size);
  }

 private:
  int id_;
  QString url_;
  Nullable<QString> mime_type_;
  Nullable<qint64> file_size_;
  bool accelerable_;
};
Q_DECLARE_METATYPE(FileSpec)

// Delete oneself after timeout?
class FileSpecGetter : public QObject {
    Q_OBJECT

 public:
  FileSpecGetter(int id, const QString& url);

 signals:
  void Error(int id, QNetworkReply::NetworkError error);
  void ResultReady(FileSpec spec);
  void Finished();

 public slots:
  void Run();

 private slots:
  void OnError(QNetworkReply::NetworkError error);
  void OnFinished();

 private:
  std::unique_ptr<QNetworkAccessManager> network_;
  std::unique_ptr<QNetworkReply> reply_;
  FileSpec spec_;
  bool error_encountered_;
};

QString ToString(SpeedGrapherState state);
SpeedGrapherState FromString(const QString& state);

// TODO(ogaro): Return const reference.
template<typename K, typename V>
V& FindOrDie(std::unordered_map<K, V>& col, const K& key) {
  auto it = col.find(key);
  if (it == col.end()) {
    std::cout << "Map lookup error: Failed to find key." << std::endl;
    exit(-1);
  }
  return it->second;
}

template<typename K, typename V>
V* FindOrNull(std::unordered_map<K, V> col, const K& key) {
  auto it = col.find(key);
  if (it == col.end()) {
    return nullptr;
  }
  V& val = it->second;
  return &val;
}

QString DirName(const QString& fpath);

QString FileName(const QString& fpath);

// Returns the file extension of the given path without the period.
QString FileExt(const QString& fpath);

QString JoinPath(const QString& pref, const QString& suff);

QString IntFileSizeToString(qint64 size);

qint64 CurrentTimeMillis();

QString StringifyTimeStamp(qint64 millis);

QString SuggestFileName(const QString& url, const QString& mime_type);

QString SuggestSaveAs(const QString& url, const QString& mime_type,
                      const QString& download_dir);

// Suffix is set to 0 if save_as does not exist.
QString MaybeAppendSuffix(const QString& save_as, int* suffix);

void HandleMessage(QtMsgType type,
                   const QMessageLogContext &context,
                   const QString &msg);

void InitApplication();

QString MakeShardPath(const QString& work_dir, const Segment& segment);
bool ParseSegment(const QString& fname, Segment* segment);
void MaybeRenameShard(qint64 actual_bytes_downloaded, QFile* shard);
void PreLaunch();
void PostLaunch();
#endif // QACCELERATOR_UTILS_H_
