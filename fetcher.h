#ifndef FETCHER_H
#define FETCHER_H
// TODO(ogaro): Investigate pause-close-resume behavior.
#include <qaccelerator-utils.h>
#include <iostream>
#include <QDir>
#include <vector>
#include <QMutex>
#include <algorithm>
#include <memory>
#include <deque>
#include <vector>
#include <QObject>
#include <QUrl>
#include <QThread>
#include <QByteArray>
#include <QFile>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QTimer>

class FetcherWorker : public QObject {
    Q_OBJECT

 public:
  FetcherWorker(int worker_id,
                qint64 pre_downloaded,
                const std::vector<Segment>& segments,
                const QUrl& url,
                const QString& work_dir,
                bool non_resume_mode);
  ~FetcherWorker();

  int GetId() {
    return worker_id_;
  }
  bool IsDone() {
    return is_done_;
  }
  bool IsInError() {
    return is_in_error_;
  }

  qint64 GetTotalDownloadedBytes() {
    return pre_downloaded_ + downloaded_;
  }
  qint64 GetTotalAllocatedBytes() {
    return pre_downloaded_ + allocation_size_;
  }
  qint64 GetPreDownloaded() {
    return pre_downloaded_;
  }
  qint64 GetDownloaded() {
    return downloaded_;
  }

 signals:
  void Completed();
  void Error(int worker_id, QNetworkReply::NetworkError code);
  void Stopped();
  void Progress(qint64 total_downloaded_);

 public slots:
  void Start();
  void Stop();

 private slots:
  void OnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void OnError(QNetworkReply::NetworkError code);
  void OnSegmentFinished();
  void UpdateProgress();

private:
  bool StartNextSegment();
  void MaybeRenameCurrentShard();

  int worker_id_;
  qint64 pre_downloaded_;
  std::vector<Segment> segments_;
  QUrl url_;  // TODO(ogaro): Do we need this?
  QString work_dir_;
  qint64 downloaded_;
  qint64 seg_bytes_received_;
  qint64 allocation_size_;
  std::vector<Segment>::iterator current_segment_;
  QNetworkRequest current_request_;
  std::unique_ptr<QNetworkAccessManager> network_;
  std::unique_ptr<QNetworkReply> current_reply_;
  std::unique_ptr<QFile> current_file_;
  bool is_done_;
  bool is_in_error_;
  bool non_resume_mode_;
  QTimer* progress_updater_;
};


class WorkerUnit : public QObject {
    Q_OBJECT
 public:
  WorkerUnit(FetcherWorker* worker);
  ~WorkerUnit();

  FetcherWorker* GetWorker();
  void Start();
  void Stop();
  bool IsDone();
  bool IsStopped();
  qint64 TotalDownloaded() { return total_downloaded_; }
  qint64 Allocation();
  int WorkerId() { return worker_id_; }

 signals:
  void Completed(int worker_id);
  void StopRequested();
  void WorkerStopped(int worker_id);
  // void Error(int worker_id, QNetworkReply::NetworkError code);

 public slots:
  void Completed();
  void OnWorkerStopped();
  void WorkerProgress(qint64 total_downloaded);
  // void OnError(QNetworkReply::NetworkError code);

 private:
  FetcherWorker* worker_;
  int worker_id_;
  QThread* thread_;
  bool is_done_;
  bool is_stopped_;
  qint64 total_downloaded_;
  qint64 byte_allocation_;
};


class Fetcher : public QObject {
    Q_OBJECT

 public:
  // TODO(ogaro): Download files of unknown size on a single thread (write
  // separate constructor for that.
  Fetcher(const QUrl& url, qint64 file_size, const QString& save_as);
  ~Fetcher();

  const QString& WorkDir();
  void Resume(const QString& work_dir, int num_connections);
  void Start(int num_connections);
  void Resume(int num_connections);
  void Stop();
  void RemoveWorkDir();
  bool GetProgress(qint64* overall_downloaded,
                   std::vector<std::pair<qint64, qint64> >* thread_stats);
  bool IsInError() { return is_in_error_; }
  void ClearError() { is_in_error_ = false; }

 signals:
  void Completed();
  void Error(QNetworkReply::NetworkError code);
  void ChangeSaveAs(const QString& new_save_as);
  void Paused();

 public slots:
  void OnWorkerStopped(int worker_id_);

 private slots:
  void RegisterCompletion(int worker_id);
  void HandleError(int worker_id, QNetworkReply::NetworkError code);

 private:
  void PrepareThreads();
  void ClearWorkerUnits();
  void MergeFiles();
  void GetDownloadedSegments(const QString& work_dir,
                             std::vector<Segment>* segments);

  // TODO(ogaro): Make `downloaded` const and assert that it is sorted. Rename
  // it to pre_downloaded
  void CalculateAllocations(std::vector<Segment>* downloaded,
                            qint64 file_size, int num_connections,
                            std::vector<std::vector<Segment> >* allocations);

  QMutex mutex_;
  QUrl url_;
  qint64 file_size_;
  QString save_as_;
  int num_connections_;  // TODO(ogaro): Remove reliance on this.
  QString work_dir_;  // TODO(ogaro): Remove reliance on this.
  QList<WorkerUnit* > worker_units_;
  bool is_in_error_;
  bool waiting_for_all_workers_stopped_;
};

#endif  // FETCHER_H
