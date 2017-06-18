#include "download-monitor-page.h"

#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QLabel>
#include <Qt>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QLayoutItem>
#include <QTabWidget>
#include <QDesktopServices>

// TODO(ogaro): Investigate what happens when starting download with 200 threads.
// TODO(ogaro): Ensure stopwatch is resumed from pause.
using std::pair;
using std::make_pair;
using std::vector;

static const int kMaxTabFNameLen = 15;
static const int kMaxSaveAsLen = 100;
static const int kMaxCentralFnameLen = 70;
static const qint64 kSpeedUpdateInterval = 500;

static const char* kPointsFname = "points.csv";

namespace {
QString Truncate(const QString& in, int max_length) {
  if (max_length < 4) {
    DIE() << "Maximum length of input string must be at least 4.";
  }
  if (in.size() <= max_length) {
    return in;
  }
  return in.mid(0, max_length - 3) + "...";
}

void ReadCoordsFromCSV(const QString& fpath,
                       vector<double>* out) {
  QFile file(fpath);
  if (file.open(QIODevice::ReadOnly)) {
    QTextStream stream(&file);
    while(!stream.atEnd()) {
      QVariant line = QVariant(stream.readLine());
      bool ok = false;
      double parsed = line.toDouble(&ok);
      if (!ok) {
        DIE() << "Failed to parse double in coords file.";
      }
      out->push_back(parsed);
    }
  } else {
    // DIE() << "Failed to open coords file for reading.";
  }
}

void DumpCoordsToCSV(const vector<double>& ys, const QString& fpath) {
  QFile file(fpath);
  if (file.open(QIODevice::WriteOnly)) {
    QTextStream stream(&file);
    for (double coord : ys) {
      stream << coord << endl;
    }
    file.close();
  } else {
    DIE() << "Failed to open coords file for writing.";
  }
}
}

// Have this take a db item and delegate from other ctors to it.
DownloadMonitorPage::DownloadMonitorPage(QWidget* parent, Session* session,
                                         PreferenceManager* preference_manager)
    : QWidget(parent),
      session_(session),
      preference_manager_(preference_manager) {
  if (parent == nullptr) {
    DIE() << "Parent of download monitor page cannot be null.";
  }
  grapher_ = new SpeedGrapher(this);
}

void DownloadMonitorPage::Init() {
  is_done_ = false;
  waiting_for_paused_ = false;
  resume_on_paused_ = false;
  cancel_on_paused_ = false;
  close_on_paused_ = false;
  if (db_item_.FileSize().Get() <= 0) {
    db_item_.SetNumConnections(1);
    non_resume_mode_ = true;
  } else {
    non_resume_mode_ = false;
  }
  preference_manager_->ApplySavedPreferences(SpeedGrapherState::IN_PROGRESS,
                                             grapher_,
                                             false);
  fname_ = FileName(db_item_.SaveAs().Get());
  progress_ = 0;
  stop_watch_.Reset(db_item_.MillisElapsed().Get());
  connect(&progress_updater_, SIGNAL(timeout()), this, SLOT(UpdateProgress()));
  prev_downloaded_bytes_ = 0;
  downloaded_bytes_ = 0;
  connect(&speed_updater_, SIGNAL(timeout()), this, SLOT(UpdateSpeed()));
  QVBoxLayout* main_layout = new QVBoxLayout(this);
  main_layout->setSpacing(0);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->setAlignment(Qt::Alignment(Qt::AlignTop | Qt::AlignLeft));
  SetUpSpeedGrapher();
  SetUpProgressStats();
  SetUpShardBox();
  SetUpNumConnectionsWidgets();
  SetUpControlBox();
  setFixedSize(sizeHint());
}

// TODO(ogaro): Merge this with other constructor.
DownloadMonitorPage::DownloadMonitorPage(QWidget* parent,
                                         Session* session,
                                         PreferenceManager* preference_manager,
                                         const DownloadItem& item)
    : DownloadMonitorPage(parent, session, preference_manager) {
  db_item_ = item;
  Init();
}

QString DownloadMonitorPage::ValidateSaveAs(const QString& save_as,
                       const QString& suggested_save_as) {
  return "";
}

void DownloadMonitorPage::SetUpSpeedGrapher() {
  QString fname = Truncate(fname_, kMaxCentralFnameLen);
  QLabel* fname_label = new QLabel(this);
  fname_label->setText(fname);
  layout()->addWidget(fname_label);
  layout()->addWidget(grapher_);
  fname_label->setContentsMargins(0, 0, 0, 0);
  grapher_->setContentsMargins(0, 0, 0, 0);
}

void DownloadMonitorPage::SetUpProgressStats() {
  QGroupBox* progress_box = new QGroupBox(this);
  progress_box->setStyleSheet(kUmemeStyle);
  QGridLayout* progress_layout = new QGridLayout(progress_box);
  progress_layout->setAlignment(Qt::Alignment(Qt::AlignLeft));
  QLabel* save_as_label = new QLabel(this);
  save_as_label->setText("Save as: ");
  progress_layout->addWidget(save_as_label, 0, 0);
  save_as_value_label_ = new QLabel(this);
  save_as_value_label_->setText(
        Truncate(db_item_.SaveAs().Get(), kMaxSaveAsLen));
  progress_layout->addWidget(save_as_value_label_, 0, 1);
  QLabel* downloaded_label = new QLabel(this);
  downloaded_label->setText("Downloaded: ");
  progress_layout->addWidget(downloaded_label, 1, 0);
  downloaded_value_label_ = new QLabel(this);
  SetDownloadedValueLabel(0);
  progress_layout->addWidget(downloaded_value_label_, 1, 1);
  download_speed_label_ = new QLabel(this);
  download_speed_label_->setText("Download speed: ");
  progress_layout->addWidget(download_speed_label_, 2, 0);
  download_speed_value_label_ = new QLabel(this);
  download_speed_value_label_->setText("0 bytes/s");
  progress_layout->addWidget(download_speed_value_label_, 2, 1);
  layout()->addWidget(progress_box);
}

void DownloadMonitorPage::SetUpShardBox() {
  shard_box_ = new QGroupBox(this);
  shard_box_->setStyleSheet(kUmemeStyle);
  QGridLayout* shard_layout = new QGridLayout(shard_box_);
  shard_layout->setAlignment(Qt::Alignment(Qt::AlignTop));
  shard_layout->setVerticalSpacing(8);
  QScrollArea* scroll_area = new QScrollArea(this);
  scroll_area->setWidgetResizable(true);
  scroll_area->setGeometry(0, 0, 650, 300);
  scroll_area->setFixedSize(scroll_area->size());
  scroll_area->setWidget(shard_box_);
  layout()->addWidget(scroll_area);
}

// TODO(ogaro): Do we need to store the gbox, label and spin as member vars?
void DownloadMonitorPage::SetUpNumConnectionsWidgets() {
  num_connections_box_ = new QGroupBox(this);
  num_connections_box_->setStyleSheet(kUmemeStyle);
  num_connections_label_ = new QLabel(this);
  num_connections_label_->setText("Number of connections");
  num_connections_spin_ = new QSpinBox(this);
  num_connections_spin_->setRange(1, kMaxConnections);
  num_connections_spin_->setValue(db_item_.NumConnections().Get());
  num_connections_spin_->setEnabled(!non_resume_mode_);
  QGridLayout* num_connections_layout = new QGridLayout();
  num_connections_layout->setAlignment(Qt::Alignment(Qt::AlignLeft));
  num_connections_button_ = new QPushButton(this);
  num_connections_button_->setText("Apply");
  QSize size_hint = num_connections_button_->sizeHint();
  if (size_hint.height() > 20) {
    num_connections_button_->setFixedSize(size_hint);
  }
  num_connections_layout->addWidget(num_connections_label_, 0, 0);
  num_connections_layout->addWidget(num_connections_spin_, 0, 1);
  num_connections_layout->addWidget(num_connections_button_, 0, 2);
  num_connections_box_->setLayout(num_connections_layout);
  connect(num_connections_button_, SIGNAL(clicked()),
          this, SLOT(ChangeNumConnections()));
  layout()->addWidget(num_connections_box_);
}

void DownloadMonitorPage::SetUpControlBox() {
  QGroupBox* control_box = new QGroupBox(this);
  control_box->setStyleSheet(kUmemeStyle);
  control_box->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,
                                         QSizePolicy::Fixed));
  QGridLayout* control_layout = new QGridLayout(control_box);
  control_layout->setAlignment(Qt::Alignment(Qt::AlignLeft));
  button1_stack_ = new QStackedWidget(this);
  pause_button_ = new QPushButton(this);
  pause_button_->setText("Pause");
  connect(pause_button_, SIGNAL(clicked()),
          this, SLOT(TogglePause()));
  pause_button_->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,
                                           QSizePolicy::Fixed));
  button1_stack_->addWidget(pause_button_);
  QPushButton* open_button = new QPushButton(this);
  open_button->setText("Open");
  connect(open_button, SIGNAL(clicked()), this, SLOT(OpenFile()));
  open_button->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,
                                         QSizePolicy::Fixed));
  button1_stack_->addWidget(open_button);
  button2_stack_ = new QStackedWidget(this);
  QPushButton* cancel_button = new QPushButton(this);
  cancel_button->setText("Cancel");
  connect(cancel_button, SIGNAL(clicked()), this, SLOT(Cancel()));
  QPushButton* open_parent_dir_button = new QPushButton(this);
  open_parent_dir_button->setText("Open Folder");
  connect(open_parent_dir_button, SIGNAL(clicked()),
          this, SLOT(OpenParentDirectory()));
  open_parent_dir_button->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,
                                                    QSizePolicy::Fixed));
  button2_stack_->addWidget(cancel_button);
  button2_stack_->addWidget(open_parent_dir_button);
  close_button_ = new QPushButton(this);
  close_button_->setText("Close");
  connect(close_button_, &QPushButton::clicked, [=] {
    emit RemoveTab(this);
  });
  close_button_->setEnabled(false);
  close_button_->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,
                                           QSizePolicy::Fixed));
  control_layout->addWidget(button1_stack_, 0, 0);
  control_layout->addWidget(button2_stack_, 0, 1);
  control_layout->addWidget(close_button_, 0, 2);
  button1_stack_->setCurrentIndex(0);
  button2_stack_->setCurrentIndex(0);
  control_layout->setContentsMargins(0, 0, 0, 0);
  layout()->addWidget(control_box);
}

void DownloadMonitorPage::DrawShardGrid() {
  QGridLayout* shard_layout = dynamic_cast<QGridLayout*>(shard_box_->layout());
  QLayoutItem* child = shard_layout->takeAt(0);
  while(child != nullptr) {
    shard_layout->removeWidget(child->widget());
    child->widget()->setParent(nullptr);
    delete child;
    child = shard_layout->takeAt(0);
  }
  shard_rows_.clear();

  for (int i = 0; i < db_item_.NumConnections().Get(); ++i) {
    shard_rows_.emplace_back(new QLabel(this), new QProgressBar(this));
    QLabel* progress_label = shard_rows_.back().Label();
    QProgressBar* bar = shard_rows_.back().ProgressBar();
    progress_label->setText("0 bytes");
    QLabel* id_label = new QLabel(this);
    id_label->setText(QString("Connection %1").arg(i + 1));
    shard_layout->addWidget(id_label, i, 0);
    shard_layout->addWidget(progress_label, i, 1);
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setMinimumHeight(10);
    bar->setMaximumHeight(10);
    shard_layout->addWidget(bar, i, 2);
  }
}

// TODO(ogaro): Separate method for resuming.
bool DownloadMonitorPage::StartDownload() {
  qDebug() << "Starting download.";
  DrawShardGrid();
  fetcher_.reset(new Fetcher(db_item_.Url().Get(),
                             db_item_.FileSize().Get(),
                             db_item_.SaveAs().Get()));
  connect(fetcher_.get(), SIGNAL(Completed()),
          this, SLOT(OnCompleted()));
  connect(fetcher_.get(), SIGNAL(Error(QNetworkReply::NetworkError)),
          this, SLOT(OnDownloadError(QNetworkReply::NetworkError)));
  connect(fetcher_.get(), &Fetcher::ChangeSaveAs,
          [=] (const QString& new_save_as) mutable {
    db_item_.SetSaveAs(new_save_as);
    fname_ = FileName(new_save_as);
    save_as_value_label_->setText(Truncate(new_save_as, kMaxSaveAsLen));
  });
  connect(fetcher_.get(), SIGNAL(Paused()), this, SLOT(OnPaused()));
  Nullable<QString> work_dir = db_item_.WorkDir();
  if (work_dir.IsNull() || work_dir.Get().isEmpty()) {
    fetcher_->Start(db_item_.NumConnections().Get());
    db_item_.SetWorkDir(fetcher_->WorkDir());
    stop_watch_.Reset(db_item_.MillisElapsed().Get());
    db_item_.SetStartTime(CurrentTimeMillis());
  } else {
    if (!QFileInfo(work_dir.Get()).exists()) {
      CHECK(QDir(work_dir.Get()).mkpath("."));
      // DIE() << "Work dir " << work_dir.Get() << " does not exist.";
    }
    vector<double> coords;
    ReadCoordsFromCSV(JoinPath(work_dir.Get(), kPointsFname), &coords);
    grapher_->SetData(coords);
    fetcher_->Resume(work_dir.Get(), db_item_.NumConnections().Get());
  }
  speed_updater_.start(kSpeedUpdateInterval);
  progress_updater_.start(kProgressUpdateInterval);
  stop_watch_.Start();
  db_item_.SetStatus(DownloadItem::StatusEnum::IN_PROGRESS);
  emit RefreshDownloadsTable();
  return true;
}

std::pair<int, double> DownloadMonitorPage::GetProgress() {
  // TODO: DELETE!!
  if (progress_ > 0.3) {
    // exit(-1);
  }
  return make_pair(db_item_.Id(), progress_);
}

void DownloadMonitorPage::Resume() {
  pause_button_->setEnabled(false);
  preference_manager_->ApplySavedPreferences(SpeedGrapherState::IN_PROGRESS,
                                             grapher_,
                                             false);
  fetcher_->Resume(db_item_.NumConnections().Get());
  stop_watch_.Start();
  speed_updater_.start(kSpeedUpdateInterval);
  progress_updater_.start(kProgressUpdateInterval);
  db_item_.SetStatus(DownloadItem::StatusEnum::IN_PROGRESS);
  emit RefreshDownloadsTable();
  pause_button_->setText("Pause");
  pause_button_->setEnabled(true);
  close_button_->setEnabled(true);
}

void DownloadMonitorPage::Pause() {
  if (waiting_for_paused_) {
    return;
  }
  waiting_for_paused_ = true;
  pause_button_->setEnabled(false);
  num_connections_box_->setEnabled(false);
  pause_button_->setText("Pausing...");
  UpdateProgress();
  progress_updater_.stop();
  speed_updater_.stop();
  stop_watch_.Stop();
  fetcher_->Stop();
}

void DownloadMonitorPage::OnPaused() {
  waiting_for_paused_ = false;
  if (speed_updater_.isActive()) {
    progress_updater_.stop();
    speed_updater_.stop();
    stop_watch_.Stop();
  }
  preference_manager_->ApplySavedPreferences(SpeedGrapherState::PAUSED,
                                             grapher_,
                                             false);
  if (cancel_on_paused_) {
    OnCancelled();
  } else if (resume_on_paused_) {
    DrawShardGrid();
    Resume();
    resume_on_paused_ = false;
  } else {
    pause_button_->setText("Resume");
    close_button_->setEnabled(true);
    db_item_.SetProgress(progress_);
    DumpCoordsToCSV(grapher_->GetData(), JoinPath(db_item_.WorkDir().Get(),
                                                  kPointsFname));
    db_item_.SetStatus(DownloadItem::StatusEnum::PAUSED);
    db_item_.SetMillisElapsed(stop_watch_.GetTimeElapsed());
    emit RefreshDownloadsTable();
    if (close_on_paused_) {
      emit RemoveTab(this);
      close_on_paused_ = false;
    }
  }
  num_connections_box_->setEnabled(true);
  pause_button_->setEnabled(true);
}

void DownloadMonitorPage::OnCancelled() {
  cancel_on_paused_ = false;
  db_item_.SetStatus(DownloadItem::StatusEnum::CANCELLED);
  db_item_.SetProgress(progress_);
  fetcher_->RemoveWorkDir();
  emit RefreshDownloadsTable();
  emit RemoveTab(this);
}

void DownloadMonitorPage::Cancel() {
  if (IsPaused()) {
    OnCancelled();
    return;
  }
  cancel_on_paused_ = true;
  Pause();
}

void DownloadMonitorPage::PauseAndClose() {
  close_on_paused_ = true;
  Pause();
}

bool DownloadMonitorPage::IsDone() {
  return is_done_;
}

bool DownloadMonitorPage::IsPaused() {
  return pause_button_->text() == "Resume";
}

void DownloadMonitorPage::SetTabProgress() {
  QString progress_string;
  if (non_resume_mode_) {
    progress_string = Truncate(fname_, kMaxTabFNameLen);
  } else {
    progress_string = QString("%1% %2")
        .arg((int) (progress_ * 100))
        .arg(Truncate(fname_, kMaxTabFNameLen));
  }
  emit SetTabProgress(this, progress_string);
}

void DownloadMonitorPage::ChangeNumConnections() {
  db_item_.SetNumConnections(num_connections_spin_->value());
  if (!IsPaused()) {
    resume_on_paused_ = true;
    Pause();
  }
}

void DownloadMonitorPage::TogglePause() {
  if (IsPaused()) {
    Resume();
  } else {
    resume_on_paused_ = false;
    Pause();
  }
}

void DownloadMonitorPage::OpenFile() {
  QDesktopServices::openUrl(QUrl::fromLocalFile(db_item_.SaveAs().Get()));
  emit RemoveTab(this);
}

void DownloadMonitorPage::OpenParentDirectory() {
  QDesktopServices::openUrl(QUrl::fromLocalFile(DirName(
     db_item_.SaveAs().Get())));
}

// TODO(ogaro): Arg not needed. Change name from set to update.
void DownloadMonitorPage::SetDownloadedValueLabel(qint64 downloaded_bytes) {
  QString display_string;
  QString str_downloaded_bytes = IntFileSizeToString(downloaded_bytes);
  if (is_done_) {
    display_string = QString("%1 in %2")
        .arg(str_downloaded_bytes)
        .arg(stop_watch_.GetTimeElapsedString());
  } else if (db_item_.FileSize().Get() <= 0) {
    display_string = QString("%1 of unknown").arg(str_downloaded_bytes);
  } else {
    display_string = QString("%1 of %2")
        .arg(str_downloaded_bytes)
        .arg(IntFileSizeToString(db_item_.FileSize().Get()));
  }
  downloaded_value_label_->setText(display_string);
}

void DownloadMonitorPage::OnCompleted() {
  is_done_ = true;
  progress_updater_.stop();
  speed_updater_.stop();
  stop_watch_.Stop();
  UpdateProgress();
  UpdateSpeed();
  button1_stack_->setCurrentIndex(1);
  button2_stack_->setCurrentIndex(1);
  close_button_->setEnabled(true);
  num_connections_box_->setEnabled(false);
  SetDownloadedValueLabel(downloaded_bytes_);
  emit SetTabProgress(this, Truncate(fname_, kMaxTabFNameLen));

  preference_manager_->ApplySavedPreferences(SpeedGrapherState::COMPLETED,
                                             grapher_,
                                             false);
  grapher_->UpdatePlot(false);
  download_speed_label_->setText("Average speed: ");
  QString avg_speed = IntFileSizeToString((int)
      (downloaded_bytes_ * 1000 / stop_watch_.GetTimeElapsed()));
  download_speed_value_label_->setText(QString("%1/s").arg(avg_speed));
  db_item_.SetStatus(DownloadItem::StatusEnum::COMPLETED);
  emit RefreshDownloadsTable();
}

void DownloadMonitorPage::OnDownloadError(QNetworkReply::NetworkError code) {
  // TODO(ogaro): Show error alert box.
  qDebug() << "Monitor received error " << code;
  if (!IsPaused()) {
    TogglePause();
    fetcher_->ClearError();
  }
}

void DownloadMonitorPage::UpdateProgress() {
  qint64 overall_downloaded = 0;
  vector<pair<qint64, qint64> > progress_tuples;
  if (!fetcher_->GetProgress(&overall_downloaded, &progress_tuples)) {
    PauseAndClose(); // This is bad.
    return;
  }
  // qDebug() << "Reported downloaded bytes is " << overall_downloaded;
  qint64 file_size = db_item_.FileSize().Get();
  if (overall_downloaded < downloaded_bytes_) {
    DIE() << "Downloaded bytes fell from " << downloaded_bytes_
          << " to " << overall_downloaded;
  }
  downloaded_bytes_ = overall_downloaded;
  SetDownloadedValueLabel(downloaded_bytes_);
  if (file_size > 0) {
    progress_ = overall_downloaded / (double) file_size;
    SetTabProgress();
  } else {
    return;
  }

  for (int i = 0; i < progress_tuples.size(); ++i) {
    qint64 shard_downloaded = progress_tuples[i].first;
    qint64 shard_allocation = progress_tuples[i].second;
    if (shard_downloaded < 0) {
      qDebug() << "Shard " << i << " had a negative file size.";
    }
    if (i >= shard_rows_.size()) {
      qDebug() << "At progress tuple " << i << " but have "
               << shard_rows_.size() << " shard rows.";
    }
    shard_rows_[i].Label()->setText(IntFileSizeToString(shard_downloaded));
    shard_rows_[i].ProgressBar()->setValue((int)
        (shard_downloaded * 100 / shard_allocation));
  }
}

void DownloadMonitorPage::UpdateSpeed() {
  if (downloaded_bytes_ == 0) {
    return;
  }
  if (prev_downloaded_bytes_ == 0) {
    prev_downloaded_bytes_ = downloaded_bytes_;
    return;
  }
  qint64 byte_delta = downloaded_bytes_ - prev_downloaded_bytes_;
  if (byte_delta < 0) {
    DIE() << "Prev downloaded is " << prev_downloaded_bytes_
          << " but downloaded is " << downloaded_bytes_;
  }
  qint64 speed = byte_delta * 1000 / kSpeedUpdateInterval;
  prev_downloaded_bytes_ += byte_delta;
  QString speed_string = QString("%1/s").arg(IntFileSizeToString(speed));
  grapher_->AddDataPoint(speed, progress_, speed_string);
  if (!is_done_) {
    download_speed_value_label_->setText(speed_string);
  }
}
