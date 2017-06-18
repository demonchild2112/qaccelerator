#ifndef DOWNLOAD_MONITOR_H_
#define DOWNLOAD_MONITOR_H_

#include "qaccelerator-db.h"
#include "qaccelerator-utils.h"
#include "download-monitor-page.h"
#include <QTabWidget>
#include <QWidget>
#include <utility>
#include <vector>
#include <QCloseEvent>
#include <QTimer>
#include <memory>

class DownloadMonitor : public QTabWidget {
    Q_OBJECT
 public:
  DownloadMonitor(QWidget* parent,
                  Session* session,
                  PreferenceManager* preference_manager);
  void MaybePopQueueFront();
  void StartOrQueueDownload(const DownloadParams& params);
  void StartOrQueueDownload(DownloadItem& item);
  void GetProgress(std::vector<std::pair<int, double> >* out);
  void CloseAndSignal();
  void QueueDownload(const DownloadParams& params);

 signals:
  void RefreshDownloadsTable();
  void AllTabsClosed();

 public slots:
  void ResumeDownload(DownloadItem& item);
  void PauseDownload(DownloadItem& item);
  void CancelDownload(DownloadItem& item);
  void AddDownload(const DownloadParams& params);
  void TransmitRefreshDownloadsTable() {
    emit RefreshDownloadsTable();
    if (!waiting_for_all_tabs_paused_) {
      MaybePopQueueFront();
    }
  }
  void SetTabProgress(DownloadMonitorPage* page, const QString& progress) {
    setTabText(indexOf(page), progress);
  }
  void RemoveTab(DownloadMonitorPage* page) {
    removeTab(indexOf(page));
    MaybeCloseOrHide();
  }

 protected:
  void closeEvent(QCloseEvent* event);

 private slots:
  void MaybeCloseOrHide();
  void OnGetterError(int id, QNetworkReply::NetworkError error);

 private:
  DownloadMonitorPage* GetTab(int db_id);
  void StartDownload(DownloadItem& item);
  void QueueDownload(DownloadItem& item);
  void BringToFront();
  bool ShouldQueueNextDownload();
  void AddDownloadTab(DownloadItem &item);
  void SetUpNewTab(DownloadMonitorPage* new_tab);

  Session* session_;
  PreferenceManager* preference_manager_;
  bool waiting_for_all_tabs_paused_;  // TODO(ogaro): Unncessary.
  bool close_and_signal_requested_;
  bool closed_;
  bool initialization_in_progress_;
  // std::unique_ptr<QTimer> timer_; // TODO(ogaro): Get rid of this.
};

#endif  // DOWNLOAD_MONITOR_H_
