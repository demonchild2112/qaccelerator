#include "qaccelerator-utils.h"

#include <QUrl>
#include <QRegularExpression>
#include <stdio.h>
#include <stdlib.h>
#include <QApplication>
#include <QLibraryInfo>
#include <QMessageBox>

// TODO(ogaro): Sanitize suggested filenames!!

const char* kContentLength = "content-length";
const char* kContentType = "content-type";
const char* kAcceptRanges = "accept-ranges";
const char* kDefaultFName = "downloaded_file";
const char* kErrorLogFName = "error.log";
const qint64 kMillisInADay = 86400000;
const qint64 kMillisInAnHour = 3600000;
const qint64 kMillisInAMinute = 60000;
const qint64 kMillisInASecond = 1000;
// const int kMaxErrorMessageAgeMillis = 7 * kMillisInADay;
const int kMaxErrorMessageAgeMillis = kMillisInAMinute * 5;

QString ToString(SpeedGrapherState state) {
  QString converted;
  switch(state) {
  case SpeedGrapherState::IN_PROGRESS:
    converted = "in_progress";
    break;
  case SpeedGrapherState::PAUSED:
    converted = "paused";
    break;
  case SpeedGrapherState::COMPLETED:
    converted = "completed";
    break;
  default:
    DIE() << "Unknown speedgrapher state";
  }
  return converted;
}

SpeedGrapherState FromString(const QString& state) {
  SpeedGrapherState converted;
  if (state == "in_progress") {
    converted = SpeedGrapherState::IN_PROGRESS;
  } else if (state == "paused") {
    converted = SpeedGrapherState::PAUSED;
  } else if (state == "completed") {
    converted = SpeedGrapherState::COMPLETED;
  } else {
    DIE() << "Unknown speedgrapher state";
  }
  return converted;
}

QString DirName(const QString& fpath) {
  QFileInfo info(fpath);
  if (info.isDir()) {
    return fpath;
  }
  return info.dir().absolutePath();
}

QString FileName(const QString& fpath) {
  return QFileInfo(fpath).fileName();
}

// Returns the file extension of the given path without the period.
QString FileExt(const QString& fpath) {
  return QFileInfo(fpath).suffix();
}

QString JoinPath(const QString& pref, const QString& suff) {
  return QDir(pref).absoluteFilePath(suff);
}

QString IntFileSizeToString(qint64 size) {
  double log_size = 0;
  if (size < 0) {
    DIE() << "Negative file size given: " << size;
  } else if (size > 0) {
    log_size = log2(size);
  }
  if (log_size < 10.0) {
    return QString("%1 bytes").arg(QString::number(size, 'f', 2));
  } else if (log_size < 20) {
    return QString("%1 KB").arg(
        QString::number(size / pow(2.0, 10.0), 'f', 2));
  } else {
    return QString("%1 MB").arg(
        QString::number(size / pow(2.0, 20.0), 'f', 2));
  }
}

qint64 CurrentTimeMillis() {
  return QDateTime::currentMSecsSinceEpoch();
}

QString StringifyTimeStamp(qint64 millis) {
  QDateTime dt = QDateTime::fromMSecsSinceEpoch(millis);
  return dt.toString("MMMM d yyyy, h:mm AP");
}

qint64 ParseDateTime(const QString& stringified) {
  QDateTime dt = QDateTime::fromString(stringified, "MMMM d yyyy, h:mm:ss AP");
  return dt.toMSecsSinceEpoch();
}

QString SuggestFileName(const QString& url, const QString& mime_type) {
  QUrl parsed(url);
  if (parsed.scheme().isEmpty() || parsed.host().isEmpty()) {
    return "";
  }

  if (mime_type.compare("text/html", Qt::CaseInsensitive) == 0) {
    // TODO(ogaro): Fetch the title of the page for the url.
    return "web_page.html";
  }

  QStringList pieces = parsed.path().split("/");
  QString fname;
  if (!pieces.isEmpty()) {
    // http://www.ietf.org/rfc/rfc3986.txt
    // Ignore text after a subdelimiter.
    QStringList sub_delims = {"!", "$", "&", "'", "(", ")",
                              "*", "+", ",", ";", "="};
    fname = pieces.last().simplified();
    QString cleaned;
    for (QChar c : fname) {
      if (sub_delims.contains(QString(c))) {
        break;
      }
      cleaned.append(c);
    }
    fname = cleaned;
  }
  if (fname.isEmpty()) {
    fname = QString(kDefaultFName);
  }
  if (FileExt(fname).isEmpty()) {
    if (mime_type.toLower() == "video/mp4") {
      QString dir = DirName(fname);
      QString baseName = QFileInfo(fname).baseName();
      QString ext = "mp4";
      fname = QString("%1%2.%3").arg(dir).arg(baseName).arg(ext);
    }
  }
  return fname;
}

QString SuggestSaveAs(const QString& url, const QString& mime_type,
                      const QString& download_dir) {
  QString fname = SuggestFileName(url, mime_type);
  QFileInfo finfo = QFileInfo(fname);
  QString prefix = finfo.baseName();
  QString suffix = finfo.suffix();
  QString path = JoinPath(download_dir, prefix + "." + suffix);
  int appended = 0;
  return MaybeAppendSuffix(path, &appended);
}

QString MaybeAppendSuffix(const QString &save_as, int* appended) {
  QString download_dir = DirName(save_as);
  QString fname = FileName(save_as);
  QFileInfo finfo = QFileInfo(fname);
  QString prefix = finfo.baseName();
  QString suffix = finfo.suffix();
  QString path;
  int i = 0;
  path = save_as;
  while(QFileInfo(path).exists()) {
    i++;
    path = JoinPath(
        download_dir,
        QString("%1 (%2)%3%4")
          .arg(prefix)
          .arg(i)
          .arg(suffix.isEmpty() ? "" : ".")
          .arg(suffix));
  };
  *appended = i;
  return path;
}

StopWatch::StopWatch()
    : start_time_(0),
      is_running_(false),
      time_before_start_(0) {
  Reset();
}

void StopWatch::Start() {
  if (is_running_) {
    DIE() << "Stopwatch has already been started.";
  }
  is_running_ = true;
  start_time_ = CurrentTimeMillis();
}

void StopWatch::Stop() {
  if (!is_running_) {
    DIE() << "Stopwatch is not running.";
  }
  time_before_start_ += CurrentTimeMillis() - start_time_;
  is_running_ = false;
}

void StopWatch::Reset(qint64 time_before_start) {
  time_before_start_ = time_before_start;
  start_time_ = 0;
  is_running_ = false;
}

void StopWatch::Reset() {
  Reset(0);
}

qint64 StopWatch::GetTimeElapsed() {
  if (is_running_) {
    return time_before_start_ + CurrentTimeMillis() - start_time_;
  } else {
    return time_before_start_;
  }
}

QString StopWatch::GetTimeElapsedString() {
  qint64 ms = GetTimeElapsed();
  qint64 days = ms / kMillisInADay;
  ms %= kMillisInADay;
  qint64 hours = ms / kMillisInAnHour;
  ms %= kMillisInAnHour;
  qint64 minutes = ms / kMillisInAMinute;
  ms %= kMillisInAMinute;
  qint64 seconds = ms / kMillisInASecond;
  QString time_string;
  if (days > 0) {
    time_string += QString("%1 day%2 ")
        .arg(days)
        .arg(days == 1 ? "" : "s");
  }
  if (hours > 0) {
    time_string += QString("%1 hour%2  ")
        .arg(hours)
        .arg(hours == 1 ? "" : "s");
  }
  if (minutes > 0) {
    time_string += QString("%1 minute%2 ")
        .arg(minutes)
        .arg(minutes == 1 ? "" : "s");
  }
  if (seconds > 0) {
    time_string += QString("%1 second%2 ")
        .arg(seconds)
        .arg(seconds == 1 ? "" : "s");
  }
  if (time_string.isEmpty()) {
    time_string = "0 seconds";
  }
  return time_string.trimmed();
}

FileSpecGetter::FileSpecGetter(int id, const QString& url)
    : spec_(id, url),
      error_encountered_(false) {}

void FileSpecGetter::Run() {
  QNetworkRequest request(spec_.Url());
  // qDebug() << "Creating network access manager.";
  network_.reset(new QNetworkAccessManager());
  reply_.reset(network_->head(request));
  // qDebug() << "Header requested.";
  connect(reply_.get(), SIGNAL(error(QNetworkReply::NetworkError)),
          this, SLOT(OnError(QNetworkReply::NetworkError)));
  connect(reply_.get(), SIGNAL(finished()), this, SLOT(OnFinished()));
}

void FileSpecGetter::OnError(QNetworkReply::NetworkError error) {
  error_encountered_ = true;
  emit Error(spec_.Id(), error);
  emit Finished();
}

void FileSpecGetter::OnFinished() {
  if (error_encountered_) {
    return;
  }
  if (reply_->hasRawHeader(kContentLength)) {
    QByteArray raw = reply_->rawHeader(kContentLength);
    bool ok;
    qint64 file_size = raw.toLongLong(&ok, 10);  // Convert to ll with base 10.
    if (ok && file_size > 0) {
      spec_.SetFileSize(file_size);
    } else {
      spec_.SetFileSize(-1); // -1 represents absence of file size.
    }
  } else {
    spec_.SetFileSize(-1); // -1 represents absence of file size.
  }
  if (reply_->hasRawHeader(kContentType)) {
    QByteArray raw = reply_->rawHeader(kContentType);
    QString content_type(raw.constData());
    if (!content_type.isEmpty()) {
      spec_.SetMimeType(content_type);
    }
  }
  if (reply_->hasRawHeader(kAcceptRanges)) {
    QByteArray raw = reply_->rawHeader(kAcceptRanges);
    bool accelerable = QString(raw.constData()).compare(
        "bytes", Qt::CaseInsensitive) == 0;
    spec_.SetAccelerable(accelerable);
  }
  emit ResultReady(spec_);
  emit Finished();
}

// Custom message handler.
void HandleMessage(QtMsgType type,
                   const QMessageLogContext &context,
                   const QString &msg) {
  QByteArray localMsg = msg.toLocal8Bit();
  QString time_format = "MMMM d yyyy, h:mm:ss AP";
  QString current_time = QDateTime::fromMSecsSinceEpoch(CurrentTimeMillis())
      .toString(time_format);;
  QString suffix;
  if (QLibraryInfo::isDebugBuild()) {
    suffix = QString(" (%s:%u, %s)")
        .arg(context.file)
        .arg(context.line)
        .arg(context.function);
  }
  switch (type) {
  case QtFatalMsg:
    fprintf(stderr,
            "%s\t FATAL: %s%s\n",
            current_time.toStdString().c_str(),
            localMsg.toStdString().c_str(),
            suffix.toStdString().c_str());
    abort();
    break;
  default:
    fprintf(stderr,
            "%s\t %s%s\n",
            current_time.toStdString().c_str(),
            localMsg.toStdString().c_str(),
            suffix.toStdString().c_str());
  }
}

void InitApplication() {
  QFile error_log(kErrorLogFName);
  // Delete old messages.
  // TODO(ogaro): Make logging thread-safe (output from threads is interleaved).
  if (error_log.exists()) {
    CHECK(error_log.open(QIODevice::ReadOnly));
    QTextStream in_stream(&error_log);
    QStringList error_lines;
    qint64 now = CurrentTimeMillis();
    bool should_rewrite_log = false;
    while (!in_stream.atEnd()) {
      QString line = in_stream.readLine();
      if (line.isEmpty()) {
        continue;
      }
      QStringList parts = line.split("\t");
      if (parts.isEmpty()) {
        // TODO(ogaro): Show error message.
        abort();
      }
      qint64 message_age = now - ParseDateTime(parts[0]);
      if (message_age > kMaxErrorMessageAgeMillis) {
        should_rewrite_log = true;
      } else if (should_rewrite_log == true) { // Old messages encountered.
        error_lines.append(line);
      }
    }
    if (!error_lines.isEmpty()) {
      error_log.remove();
      CHECK(error_log.open(QIODevice::WriteOnly));
      QTextStream out_stream(&error_log);
      for (const QString& line : error_lines) {
        out_stream << line << "\n";
      }
    }
  } else {
    CHECK(error_log.open(QIODevice::WriteOnly));
  }
  error_log.close();
  freopen(kErrorLogFName, "a", stderr);
  qInstallMessageHandler(HandleMessage);
}

QString CategoryToString(const Category& category) {
  QString stringified;
  switch(category) {
  case Category::VIDEOS:
    stringified = "Videos";
    break;
  case Category::IMAGES:
    stringified = "Images";
    break;
  case Category::AUDIO:
    stringified = "Audio";
    break;
  case Category::DOCUMENTS:
    stringified = "Documents";
    break;
  case Category::SOFTWARE:
    stringified = "Software";
    break;
  case Category::OTHER:
    stringified = "Other";
    break;
  default:
    // Control should never reach here!
    DIE() << "Unrecognized category enum.";
  }
  return stringified;
}

Category StringToCategory(const QString& category) {
  Category parsed;
  if (category == "Videos") {
    parsed = Category::VIDEOS;
  } else if (category == "Images") {
    parsed = Category::IMAGES;
  } else if (category == "Audio") {
    parsed = Category::AUDIO;
  } else if (category == "Documents") {
    parsed = Category::DOCUMENTS;
  } else if (category == "Software") {
    parsed = Category::SOFTWARE;
  } else if (category == "Other") {
    parsed = Category::OTHER;
  } else {
    DIE() << "Unrecognized category " << category;
  }
  return parsed;
}

QString MakeShardPath(const QString& work_dir, const Segment& segment) {
  return QDir(work_dir).absoluteFilePath(
        QString("shard_%1_%2").arg(segment.first).arg(segment.second));
}

/**
 * Returns true if parse is successful.
 */
bool ParseSegment(const QString& fname, Segment* segment) {
  QStringList pieces = fname.split("_", QString::SkipEmptyParts);
  if (pieces.size() != 3) {
    return false;
  }
  bool start_ok, end_ok;
  segment->first = pieces.at(1).toLongLong(&start_ok);
  segment->second = pieces.at(2).toLongLong(&end_ok);
  return start_ok && end_ok;
}

void MaybeRenameShard(qint64 actual_bytes_downloaded, QFile* shard) {
  Segment expected_downloaded_segment;
  QFileInfo finfo = QFileInfo(*shard);
  CHECK(ParseSegment(finfo.fileName(), &expected_downloaded_segment));
  if (actual_bytes_downloaded <= 0) {
    CHECK(shard->remove());
    return;
  }
  qint64 start = expected_downloaded_segment.first;
  Segment actual_downloaded_segment =
      Segment(start, start + actual_bytes_downloaded - 1);
  if (actual_downloaded_segment.second == expected_downloaded_segment.second) {
    // No rename necessary.
    return;
  }
  QString work_dir = finfo.dir().absolutePath();
  QString new_shard_path = MakeShardPath(work_dir, actual_downloaded_segment);
  if (QFile::exists(new_shard_path)) {
    QFile::remove(new_shard_path);
  }
  if (!shard->rename(new_shard_path)) {
    DIE() << "Shard rename to " << new_shard_path << " failed";
  }
}

void PreLaunch() {
  if (QFile::exists(kLaunchFName)) {
    QFile::remove(kDbFName);  // Previous launch attempt failed.
    QMessageBox::information(
        0,
        "Download history lost",
        "An error occurred, and your recent download history has been lost. "
        "Sorry about that. :(");
  } else {
    QFile launch_file(kLaunchFName);
    launch_file.open(QIODevice::WriteOnly);
    launch_file.close();
  }
}

void PostLaunch() {
  QFile::remove(kLaunchFName);
}
