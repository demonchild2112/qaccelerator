#include "fetcher.h"

#include <QDir>

using std::pair;
using std::vector;

static const char * kNonResumeModeFileName = "NON_RESUMABLE";

namespace {
struct AscendingStartIndex {
  inline bool operator() (const Segment& a, const Segment& b) {
    return a.first < b.first;
  }
};

void Sort(std::vector<Segment>* segments) {
  std::sort(segments->begin(), segments->end(), AscendingStartIndex());
}

qint64 CountBytes(const std::vector<Segment>& segments) {
  qint64 sum = 0;
  for (const Segment& segment : segments) {
    sum += segment.second - segment.first + 1;
  }
  return sum;
}
}

FetcherWorker::FetcherWorker(int worker_id,
                             qint64 pre_downloaded,
                             const vector<Segment>& segments,
                             const QUrl& url,
                             const QString& work_dir,
                             bool non_resume_mode)
    : worker_id_(worker_id),
      pre_downloaded_(pre_downloaded),
      segments_(segments),
      url_(url),
      work_dir_(work_dir),
      downloaded_(0),
      seg_bytes_received_(0),
      allocation_size_(CountBytes(segments)),
      current_segment_(segments_.begin()),
      current_request_(url),
      non_resume_mode_(non_resume_mode) {
  is_done_ = false;
  is_in_error_ = false;
  progress_updater_ = new QTimer(this);
  connect(progress_updater_, SIGNAL(timeout()), this, SLOT(UpdateProgress()));
  current_request_.setRawHeader("connection", "Keep-Alive");
  current_request_.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute,
                                true);
}

FetcherWorker::~FetcherWorker() {
  if (current_reply_ != nullptr) {
    Stop();
  }
}

void FetcherWorker::UpdateProgress() {
  emit Progress(GetTotalDownloadedBytes());
}

void FetcherWorker::Start() {
  network_.reset(new QNetworkAccessManager());
  progress_updater_->start(kProgressUpdateInterval);
  if (!StartNextSegment() && !IsInError()) {
    is_done_ = true;
    progress_updater_->stop();
    emit Completed();
  }
  // qDebug() << "Worker " << worker_id_ << " started.";
}

// TODO(ogaro): Confirm content length?
bool FetcherWorker::StartNextSegment() {
  if (!non_resume_mode_ && current_segment_ == segments_.end()) {
    return false;
  }
  QString fname;
  if (non_resume_mode_) {
    fname = JoinPath(work_dir_, kNonResumeModeFileName);
  } else {
    fname = MakeShardPath(work_dir_, *current_segment_);
  }

  current_file_.reset(new QFile(fname));
  if (!current_file_->open(QIODevice::WriteOnly)) {
    // TODO(ogaro): Emit an error to controller.
    DIE() << "Worker " << worker_id_ << " failed to open shard file "
          << fname;
    is_in_error_ = true;
    return false;
  }

  seg_bytes_received_ = 0;
  if (!non_resume_mode_) {
    QString range_header = QString("bytes=%1-%2")
        .arg(current_segment_->first)
        .arg(current_segment_->second);
    current_request_.setRawHeader("range", range_header.toUtf8());
  }
  current_reply_.reset(network_->get(current_request_));
  connect(current_reply_.get(), SIGNAL(finished()),
          this, SLOT(OnSegmentFinished()));
  connect(current_reply_.get(), SIGNAL(downloadProgress(qint64, qint64)),
          this, SLOT(OnDownloadProgress(qint64, qint64)));
  //connect(current_reply_.get(), SIGNAL(error(QNetworkReply::NetworkError)),
  //        this, SLOT(OnError(QNetworkReply::NetworkError)));
  return true;
}

void FetcherWorker::Stop() {
  disconnect(current_reply_.get(), 0, 0, 0);

  current_reply_->abort();
  if (seg_bytes_received_ > 0) {
    MaybeRenameCurrentShard();
  } else {
    current_file_->remove(); // Nothing downloaded for this segment.
  }

  current_file_->close();
  current_file_.reset();
  current_reply_.reset();
  progress_updater_->stop();
  UpdateProgress();
  emit Stopped();
}

void FetcherWorker::MaybeRenameCurrentShard() {
  MaybeRenameShard(seg_bytes_received_, current_file_.get());
  /*qint64 start = current_segment_->first;
  if (seg_bytes_received_ <= 0) {
    CHECK(current_file_->remove());
    return;
  }
  Segment done_so_far = Segment(start, start + seg_bytes_received_ - 1);
  if (done_so_far.second == current_segment_->second) {
    // No rename necessary.
    return;
  }
  QString new_shard_path = MakeShardPath(work_dir_, done_so_far);
  if (QFile::exists(new_shard_path)) {
    QFile::remove(new_shard_path);
  }
  if (!current_file_->rename(new_shard_path)) {
    DIE() << "Shard rename to " << new_shard_path << " failed";
  }*/
}

void FetcherWorker::OnSegmentFinished() {
  disconnect(current_reply_.get(), 0, 0, 0);
  if (non_resume_mode_) {
    // Rename file so it can be merged later.
    QString new_shard_path = MakeShardPath(
          work_dir_, Segment(0, downloaded_ - 1));
    if (!current_file_->rename(new_shard_path)) {
      DIE() << "Shard rename to " << new_shard_path << " failed";
    }
    is_done_ = true;
  }

  if (!non_resume_mode_) {
    MaybeRenameCurrentShard();
    current_file_->close();
    current_file_.reset();
    current_reply_.reset();
    ++current_segment_;
    if (!StartNextSegment() && !IsInError()) {
      is_done_ = true;
    }
  } else {
    current_file_->close();
    current_file_.reset();
    current_reply_.reset();
  }
  if (is_done_) {
    progress_updater_->stop();
    UpdateProgress();
    emit Completed();
  }
}

// TODO(ogaro): Double-check whether it is ok to assume that bytesReceived
// should always add up to the reported size of a file.
void FetcherWorker::OnDownloadProgress(qint64 bytesReceived,
                                       qint64 bytesTotal) {
  if (bytesReceived < seg_bytes_received_) {
    DIE() << "Bytes received was " << bytesReceived
             << " but seg bytes received was" << seg_bytes_received_;
  }
  qint64 delta = bytesReceived - seg_bytes_received_;
  seg_bytes_received_ += delta;
  downloaded_ += delta;
  if (current_file_ == nullptr) {
    // TODO(ogaro): Emit error.
    DIE()<< "OnDownloadProgress encountered null file pointer.";
    return;
  }
  current_file_->write(current_reply_->readAll());
}

void FetcherWorker::OnError(QNetworkReply::NetworkError code) {
  qDebug() << "Worker " << worker_id_ << " encountered error " << code;
  // TODO(ogaro): Write a function that converts the code to string. Don't
  // know if calling current_reply_->errorString() will be appropriate here.
  is_in_error_ = true;
  /*
  QString err;
  if (current_reply_ != nullptr) {
    err = current_reply_->errorString();
  }*/
  progress_updater_->stop();
  emit Error(worker_id_, code);
}

WorkerUnit::WorkerUnit(FetcherWorker* worker)
      : worker_(worker),
        worker_id_(worker->GetId()),
        thread_(new QThread()),
        is_done_(false),
        byte_allocation_(worker->GetTotalAllocatedBytes()),
        total_downloaded_(worker->GetPreDownloaded()) {
  is_stopped_ = false;
  worker_->moveToThread(thread_);
  // connect(worker_, SIGNAL(Error(QNetworkReply::NetworkError)),
  //        this, SLOT(OnError(QNetworkReply::NetworkError)));
  connect(this, SIGNAL(StopRequested()), worker_, SLOT(Stop()));
  connect(worker_, SIGNAL(Progress(qint64)), this, SLOT(WorkerProgress(qint64)));
  connect(worker_, SIGNAL(Stopped()), this, SLOT(OnWorkerStopped()));
  connect(thread_, SIGNAL(started()), worker_, SLOT(Start()));
  connect(worker_, SIGNAL(Completed()), this, SLOT(Completed()));
  // connect(worker_, SIGNAL(Completed()), thread_, SLOT(quit()));
  connect(thread_, SIGNAL(finished()), thread_, SLOT(deleteLater()));
  connect(worker_, SIGNAL(Completed()), worker_, SLOT(deleteLater()));
}

WorkerUnit::~WorkerUnit() {
  if (!is_done_) {
    // worker_->deleteLater();
    // thread_->deleteLater();
  }
}

FetcherWorker* WorkerUnit::GetWorker() {
  return worker_;
}

void WorkerUnit::WorkerProgress(qint64 total_downloaded) {
  total_downloaded_ = total_downloaded;
  // qDebug() << "Worker " << worker_id_ << " has downloaded " << total_downloaded;
}

void WorkerUnit::Start() {
  thread_->start();
}

void WorkerUnit::Stop() {
  if (!worker_->IsDone() || !worker_->IsInError()) {
    emit StopRequested();
  }
}

void WorkerUnit::OnWorkerStopped() {
  thread_->quit();
  thread_->deleteLater();
  worker_->deleteLater();
  is_stopped_ = true;
  emit WorkerStopped(worker_id_);
}

bool WorkerUnit::IsDone() {
  return is_done_;
}

bool WorkerUnit::IsStopped() {
  return is_stopped_;
}

qint64 WorkerUnit::Allocation() {
  return byte_allocation_;
}

void WorkerUnit::Completed() {
  is_done_ = true;
  emit Completed(worker_id_);
}

/*
void WorkerUnit::OnError(QNetworkReply::NetworkError code) {
  emit Error(worker_id_, code);
  thread_->quit();
}
*/

Fetcher::Fetcher(const QUrl& url, qint64 file_size, const QString& save_as)
      : url_(url),
        file_size_(file_size),
        save_as_(save_as),
        num_connections_(0),
        work_dir_("") {
  is_in_error_ = false;
  waiting_for_all_workers_stopped_ = false;
  CHECK(!save_as.isEmpty());
  // Preconditions: Overwrite save-as if preferences say so, or raise an
  // error.
}

Fetcher::~Fetcher() {
  ClearWorkerUnits();
}

const QString& Fetcher::WorkDir() {
  return work_dir_;
}

void Fetcher::Resume(const QString& work_dir, int num_connections) {
  work_dir_ = work_dir;
  Resume(num_connections);
}

void Fetcher::Start(int num_connections) {
  int appended = 0;
  QString new_save_as = MaybeAppendSuffix(save_as_, &appended);
  if (appended > 0) { // Avoid overwriting existing files.
    save_as_ = new_save_as;
    emit ChangeSaveAs(new_save_as);
  }
  QString download_dir = DirName(save_as_);
  work_dir_ = save_as_ + ".qaccelerator";
  int i = 0;
  QString fname = FileName(save_as_);
  QFileInfo finfo = QFileInfo(fname);
  QString prefix = finfo.baseName();
  QString suffix = finfo.suffix();
  while (QFileInfo(work_dir_).exists()) {
    ++i;
    save_as_ = JoinPath(
        download_dir,
        QString("%1 (%2)%3%4")
          .arg(prefix)
          .arg(i)
          .arg(suffix.isEmpty() ? "" : ".")
          .arg(suffix));
    work_dir_ = save_as_ + ".qaccelerator";
  }
  if (i > 0) {
    emit ChangeSaveAs(save_as_);
  }
  Resume(num_connections);
}

void Fetcher::Resume(int num_connections) {
  CHECK(!work_dir_.isEmpty());
  if (file_size_ < 1) {
    CHECK(num_connections == 1);
  }
  num_connections_ = num_connections;
  QDir dir(work_dir_);
  if (file_size_ < 1) { // Unknown file size.
    if (dir.exists()) {
      CHECK(dir.removeRecursively());
    }
    CHECK(dir.mkpath("."));
  } else {
    if (!dir.exists()) {
      CHECK(dir.mkpath("."));
      // qDebug() << "Work dir " << work_dir_ << " created.";
    }
  }
  PrepareThreads();
  for (WorkerUnit* unit : worker_units_) {
    unit->Start();
  }
}

void Fetcher::PrepareThreads() {
  qDebug() << "File size is " << file_size_;
  std::vector<Segment> pre_downloaded_segments;
  GetDownloadedSegments(work_dir_, &pre_downloaded_segments);
  std::vector<std::vector<Segment> > allocations;
  if (file_size_ > 0) {
    CalculateAllocations(&pre_downloaded_segments, file_size_, num_connections_,
                         &allocations);
  } else {
    vector<Segment> empty_alloc = { Segment(0, 0) };
    allocations.push_back(empty_alloc);
  }
  qint64 pre_downloaded_bytes = CountBytes(pre_downloaded_segments);
  qint64 pre_downloaded_per_worker = pre_downloaded_bytes / num_connections_;
  for (int i = 0; i < num_connections_; ++i) {
    qint64 pre_downloaded_for_worker = pre_downloaded_per_worker;
    if (i == num_connections_ - 1) {
      pre_downloaded_for_worker += pre_downloaded_bytes % num_connections_;
    }
    FetcherWorker* worker = new FetcherWorker(
        i,
        pre_downloaded_for_worker,
        allocations[i],
        url_,
        work_dir_,
        file_size_ <= 0);
    // () << "Worker is at " << reinterpret_cast<qint64>(worker);
    WorkerUnit* worker_unit = new WorkerUnit(worker);
    // qDebug() << "Worker unit is at " << reinterpret_cast<qint64>(worker_unit);
    connect(worker_unit, SIGNAL(WorkerStopped(int)), this, SLOT(OnWorkerStopped(int)));
    connect(worker_unit, SIGNAL(Completed(int)),
            this, SLOT(RegisterCompletion(int)));
    connect(worker, SIGNAL(Error(int, QNetworkReply::NetworkError)),
            this, SLOT(HandleError(int, QNetworkReply::NetworkError)));
    worker_units_.append(worker_unit);
  }
}

void Fetcher::HandleError(int worker_id, QNetworkReply::NetworkError code) {
  qDebug() << "Thread " << worker_id << " encountered " << code;
  if (!is_in_error_) {
    is_in_error_ = true;
    emit Error(code); // Only need to emit an error once.
  }
}

void Fetcher::Stop() {
  waiting_for_all_workers_stopped_ = true;
  for (const auto& unit : worker_units_) {
    if (!unit->IsDone()) {
      unit->Stop();
    }
  }
}

void Fetcher::OnWorkerStopped(int worker_id) {
  bool all_workers_stopped = true;
  for (const auto& unit : worker_units_) {
    if (!unit->IsDone() && !unit->IsStopped()) {
      all_workers_stopped = false;
      break;
    }
  }
  if (all_workers_stopped) {
    ClearWorkerUnits();
    emit Paused();
    waiting_for_all_workers_stopped_ = false;
  }
}

void Fetcher::RemoveWorkDir() {
  if (work_dir_.isEmpty()) {
    return;
  }
  QDir(work_dir_).removeRecursively();
}

void Fetcher::ClearWorkerUnits() {
  for (WorkerUnit* worker_unit : worker_units_) {
    worker_unit->deleteLater();
  }
  worker_units_.clear();
}

bool Fetcher::GetProgress(qint64* overall_downloaded,
                 std::vector<std::pair<qint64, qint64> >* thread_stats) {
  *overall_downloaded = 0;
  for (WorkerUnit* unit : worker_units_) {
    qint64 downloaded = unit->TotalDownloaded();
    // Note: This will be 1 if file size is unknown.
    qint64 allocated = unit->Allocation();
    if (downloaded < 0) {
      DIE() << "Running worker " << unit->GetWorker()->GetId()
            << " had negative downloaded value "
            << downloaded;
    } else if (file_size_ > 0 && downloaded > allocated) {
      qDebug() << downloaded << " (downloaded) is greater than "
               << allocated << " (allocated).";
      return false;
    }
    *overall_downloaded += downloaded;
    if (*overall_downloaded < 0) {
      DIE() << "Overall downloaded fell to less than zero.";
    }
    thread_stats->push_back(std::make_pair(downloaded, allocated));
  }
  return true;
}

void Fetcher::RegisterCompletion(int worker_id) {
  // TODO(ogaro): QMutexLocker?
  mutex_.lock();
  bool all_completed = true;
  int num_completed = 0; // TODO(ogaro): Remove this counter;
  for (WorkerUnit* unit : worker_units_) {
    if (!unit->IsDone()) {
      all_completed = false;
      break;
    }
    ++num_completed;
  }
  if (all_completed) {
    qint64 overall_downloaded = 0;
    std::vector<std::pair<qint64, qint64> > thread_stats;
    GetProgress(&overall_downloaded, &thread_stats);
    if (overall_downloaded < file_size_) {
      qDebug() << "Sending paused signal.";
      ClearWorkerUnits();
      emit Paused();
    } else {
      MergeFiles();
      // TODO(ogaro): At this point, not all QThreads may have been destroyed.
      emit Completed();
    }
  }
  mutex_.unlock();
}

void Fetcher::MergeFiles() {
  std::vector<Segment> downloaded_segments;
  GetDownloadedSegments(work_dir_, &downloaded_segments);
  Sort(&downloaded_segments);

  // TODO(ogaro): Check here that the segments are valid.

  QFile merged(save_as_);
  if (!merged.open(QIODevice::WriteOnly)) {
    // TODO(ogaro): Send an error.
    qDebug() << "Failed to open file " << save_as_;
    return;
  }
  int buffer_size = 2048;

  for (const Segment& segment : downloaded_segments) {
    QString shard_path = MakeShardPath(work_dir_, segment);
    QFile shard(shard_path);
    if (!shard.open(QIODevice::ReadOnly)) {
      // TODO(ogaro): Send an error.
      qDebug() << "Failed to open file " << shard_path;
      return;
    }
    while(!shard.atEnd()) {
      QByteArray buffer = shard.read(buffer_size);
      if (buffer.size() == 0) {
        // TODO(ogaro): Log error (shard.errorString());
        continue;
      }
      if (merged.write(buffer) == -1) {
        // TODO(ogaro): Log error (merged.errorString());
      }
    }
    shard.close();
  }
  QDir(work_dir_).removeRecursively();
}

void Fetcher::GetDownloadedSegments(const QString& work_dir,
                           std::vector<Segment>* segments) {
  QStringList fnames = QDir(work_dir).entryList(
      QDir::NoDotAndDotDot | QDir::Files);
  foreach(const QString& fname, fnames) {
    Segment segment;
    if (!ParseSegment(fname, &segment)) {
      continue;
    }
    segments->push_back(segment);
  }
}

void Fetcher::CalculateAllocations(std::vector<Segment>* downloaded,
                          qint64 file_size, int num_connections,
                          std::vector<std::vector<Segment> >* allocations) {
  qint64 undownloaded_size = file_size - CountBytes(*downloaded);
  for (const Segment& segment : *downloaded) {
    if (segment.first > segment.second) {
      DIE() << "Segment (" << segment.first << ", " << segment.second
            << ") is invalid.";
    }
  }
  if (undownloaded_size < 0) {
    DIE() << __FUNCTION__ << "File size is " << file_size
          << " but number of undownloaded is " << undownloaded_size;
  }
  Sort(downloaded);

  // Get the boundaries of the undownloaded segments of the file.
  std::deque<Segment> undownloaded;
  qint64 start = 0;
  qint64 end = 0;
  for (int i = 0; i < downloaded->size(); ++i) {
    end = downloaded->at(i).first - 1;
    if (end - start + 1 > 0) {
      undownloaded.push_back(Segment(start, end));
    }
    start = downloaded->at(i).second + 1;
  }
  end = file_size - 1;
  if (end - start + 1 > 0) {
    undownloaded.push_back(Segment(start, end));
  }

  qint64 min_thread_load = undownloaded_size / num_connections;
  for (int i = 0; i < num_connections; ++i) {
    qint64 thread_load = min_thread_load;
    std::vector<Segment> allocation;
    if (i == num_connections - 1) {
      thread_load += undownloaded_size % num_connections;
    }
    if (thread_load == 0) {
      allocations->push_back(allocation);
      continue;
    }
    qint64 capacity_left = thread_load;
    while (undownloaded.size() > 0) {
      Segment segment = undownloaded.at(0);
      undownloaded.pop_front();
      qint64 start = segment.first;
      qint64 end = segment.second;
      qint64 segment_size = end - start + 1;

      if (segment_size < capacity_left) {
        allocation.push_back(segment);
        capacity_left -= segment_size;
      } else if (segment_size == capacity_left) {
        allocation.push_back(segment);
        break;  // We are done allocating for thread i.
      } else {
        // Split segment.
        allocation.push_back(Segment(start, start + capacity_left - 1));
        undownloaded.push_front(Segment(start + capacity_left, end));
        break;
      }
    }
    allocations->push_back(allocation);
  }
}
