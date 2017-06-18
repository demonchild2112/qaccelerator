#include "download-monitor.h"

#include <QVBoxLayout>
#include <QFile>
#include <QMessageBox>

DownloadMonitor::DownloadMonitor(QWidget* parent,
                                 Session* session,
                                 PreferenceManager* preference_manager)
    : QTabWidget(parent),
      session_(session),
      preference_manager_(preference_manager),
      waiting_for_all_tabs_paused_(false),
      close_and_signal_requested_(false),
      closed_(false) {
  initialization_in_progress_ = false;
  setWindowFlags(Qt::Window);
  setWindowTitle("QAccelerator");
  setWindowIcon(QIcon(":/images/qx_flash.png"));
  hide();
}

void DownloadMonitor::MaybeCloseOrHide() {
  if (count() >= 1) {
    return;
  }
  if (waiting_for_all_tabs_paused_) {
    close();
    if (close_and_signal_requested_) {
      emit AllTabsClosed();
    }
  } else {
    hide();
  }
}

DownloadMonitorPage* DownloadMonitor::GetTab(int db_id) {
  for (int i = 0; i < count(); ++i) {
    DownloadMonitorPage* tab = dynamic_cast<DownloadMonitorPage*>(widget(i));
    if (tab->DBItem().Id() == db_id) {
      return tab;
    }
  }
  return nullptr;
}

void DownloadMonitor::GetProgress(std::vector<std::pair<int, double> >* out) {
  for (int i = 0; i < count(); ++i) {
    DownloadMonitorPage* tab = dynamic_cast<DownloadMonitorPage*>(widget(i));
    out->push_back(tab->GetProgress());
  }
}

void DownloadMonitor::ResumeDownload(DownloadItem &item) {
  DownloadMonitorPage* tab = GetTab(item.Id());
  if (tab == nullptr) {
    StartOrQueueDownload(item);
  } else {
    tab->Resume();
  }
}

void DownloadMonitor::PauseDownload(DownloadItem &item) {
  DownloadMonitorPage* tab = GetTab(item.Id());
  if (tab != nullptr) {
    tab->Pause();
  }
}

void DownloadMonitor::CancelDownload(DownloadItem &item) {
  DownloadMonitorPage* tab = GetTab(item.Id());
  if (tab == nullptr) {
    Nullable<QString> work_dir = item.WorkDir();
    if (!work_dir.IsNull() && QFile::exists(work_dir.Get())) {
      QDir(work_dir.Get()).removeRecursively();
    }
  } else {
    if (!tab->IsDone()) {
      tab->Cancel();
    }
  }
}

void DownloadMonitor::AddDownload(const DownloadParams &params) {
  StartOrQueueDownload(params);
}

void DownloadMonitor::QueueDownload(DownloadItem &item) {
  item.SetStatus(DownloadItem::StatusEnum::QUEUED);
  emit RefreshDownloadsTable();
}

void DownloadMonitor::QueueDownload(const DownloadParams &params) {
  Nullable<DownloadItem> item = DownloadItem::AddNew({
      {"url", params.Url()},
      {"save_as", params.SaveAs()},
      {"file_size", params.FileSize()},
      {"num_connections", params.NumConnections()}
  }, session_);
  if (item.IsNull()) {
    DIE() << "Failed to create DownloadItem in db.";
  }
  QueueDownload(item.Get());
}

void DownloadMonitor::MaybePopQueueFront() {
  typedef DownloadItem::StatusEnum Status;
  Nullable<DownloadItem> front = DownloadItem::Get(
      session_, "status", DownloadItem::ToInt(Status::QUEUED));
  if (!front.IsNull()) {
    StartOrQueueDownload(front.Get());
  }
}

bool DownloadMonitor::ShouldQueueNextDownload() {
  // Only one download should be initializing at any given time.
  if (initialization_in_progress_) {
    return true;
  }
  int num_downloads_in_progress = 0;
  for (int i = 0; i < count(); ++i) {
    DownloadMonitorPage* tab = static_cast<DownloadMonitorPage*>(widget(i));
    if (!tab->IsPaused() && !tab->IsDone()) {
      ++num_downloads_in_progress;
    }
  }
  int concurrent_cap;
  preference_manager_->Get("concurrent_cap", &concurrent_cap);
  return (num_downloads_in_progress >= concurrent_cap);
}

void DownloadMonitor::StartDownload(DownloadItem &item) {
  initialization_in_progress_ = true;
  Nullable<qint64> file_size = item.FileSize();
  // qDebug() << "Starting " << item.Url().Get();
  if (file_size.IsNull() || file_size.Get() == 0
      || item.SaveAs().Get().isEmpty()) { // Fetch file specs
    FileSpecGetter* getter = new FileSpecGetter(0, item.Url().Get());
    // qDebug() << "Getter created.";
    connect(getter, &FileSpecGetter::ResultReady,
            [=] (const FileSpec& spec) mutable {
      // qDebug() << "Received result ready from getter.";
      CHECK(spec.FileSize().Get() != 0); // File size should be -1 or > 0
      item.SetFileSize(spec.FileSize().Get());
      Nullable<QString> save_as = item.SaveAs();
      if (save_as.IsNull() || save_as.Get().isEmpty()) {
        QString download_dir;
        QString mime_type;
        if (!spec.MimeType().IsNull()) {
          mime_type = spec.MimeType().Get();
        }
        preference_manager_->Get("download_dir", &download_dir);
        item.SetSaveAs(SuggestSaveAs(spec.Url(), mime_type , download_dir));
      }
      Nullable<int> num_connections = item.NumConnections();
      if (num_connections.IsNull() || num_connections.Get() < 1) {
        int default_num_connections = 0;
        preference_manager_->Get("num_connections", &default_num_connections);
        item.SetNumConnections(default_num_connections);
      }
      // qDebug() << "Starting or queueing download again.";
      StartDownload(item);
    });
    connect(getter, &FileSpecGetter::Error,
            [=] (int index, QNetworkReply::NetworkError error) mutable {
      qDebug() << error;
      initialization_in_progress_ = false;
      item.SetStatus(DownloadItem::StatusEnum::FAILED);
      emit RefreshDownloadsTable();
      MaybePopQueueFront();
      MaybeCloseOrHide();
    });
    connect(getter, SIGNAL(Finished()), getter, SLOT(deleteLater()));
    getter->Run();
    return;
  }
  // qDebug() << "Retrieved spec for " << item.Url().Get();
  QString save_as_dir = DirName(item.SaveAs().Get());
  if (!QFileInfo(save_as_dir).exists()) {
    item.SetStatus(DownloadItem::StatusEnum::FAILED);
    emit RefreshDownloadsTable();
    QMessageBox::critical(
        this,
        "Directory does not exist",
        QString("The directory %1 does not exist.").arg(save_as_dir));
    MaybeCloseOrHide();
    initialization_in_progress_ = false;
    MaybePopQueueFront();
    return;
  }
  AddDownloadTab(item);
  initialization_in_progress_ = false;
  MaybePopQueueFront();
}

void DownloadMonitor::StartOrQueueDownload(DownloadItem &item) {
  CHECK(!item.Url().Get().isEmpty());
  if (ShouldQueueNextDownload()) {
    QueueDownload(item);
  } else {
    StartDownload(item);
  }
  // timer_.reset(new QTimer());
}

void DownloadMonitor::StartOrQueueDownload(const DownloadParams &params) {
  QString url = params.Url();
  QString save_as = params.SaveAs();
  int num_connections = params.NumConnections();
  qint64 file_size = params.FileSize();
  StartOrQueueDownload(DownloadItem::AddNew({
      {"url", url},
      {"save_as", save_as},
      {"num_connections", num_connections},
      {"file_size", file_size},
  }, session_).Get());
}

void DownloadMonitor::OnGetterError(int id, QNetworkReply::NetworkError error) {
  qDebug() << "Monitor encountered error " << error;
}

void DownloadMonitor::AddDownloadTab(DownloadItem &item) {
  DownloadMonitorPage* new_tab = new DownloadMonitorPage(
      this, session_, preference_manager_, item);
  SetUpNewTab(new_tab);
}

void DownloadMonitor::SetUpNewTab(DownloadMonitorPage* new_tab) {
  connect(new_tab, SIGNAL(RefreshDownloadsTable()),
          this, SLOT(TransmitRefreshDownloadsTable()));
  connect(new_tab, SIGNAL(SetTabProgress(DownloadMonitorPage*,QString)),
          this, SLOT(SetTabProgress(DownloadMonitorPage*,QString)));
  connect(new_tab, SIGNAL(RemoveTab(DownloadMonitorPage*)),
          this, SLOT(RemoveTab(DownloadMonitorPage*)));
  setCurrentIndex(addTab(new_tab, ""));
  if (count() <= 1) {
    setFixedSize(sizeHint());
  }
  new_tab->SetTabProgress();
  new_tab->StartDownload();
  closed_ = false;
  show();
  BringToFront();
}

void DownloadMonitor::CloseAndSignal() {
  close_and_signal_requested_ = true;
  close();
}

void DownloadMonitor::BringToFront() {
  setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
  raise();  // Mac
  activateWindow();  // Windows
}

void DownloadMonitor::closeEvent(QCloseEvent* event) {
  hide();
  waiting_for_all_tabs_paused_ = true;
  for (int i = 0; i < count(); ++i) {
    DownloadMonitorPage* tab = static_cast<DownloadMonitorPage*>(widget(i));
    if (tab == nullptr) {
      continue;
    }
    if (!tab->IsPaused() && !tab->IsDone()) {
      tab->PauseAndClose();
    } else {
      RemoveTab(tab);
    }
  }
  if (count() < 1) {
    event->accept();
    closed_ = true;
  } else {
    event->ignore();
  }
}
