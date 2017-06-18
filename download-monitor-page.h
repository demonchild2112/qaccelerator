#ifndef DOWNLOAD_MONITOR_PAGE_H_
#define DOWNLOAD_MONITOR_PAGE_H_

#include "qaccelerator-db.h"
#include "qaccelerator-utils.h"
#include "speed-grapher.h"
#include "fetcher.h"
#include <utility>
#include <vector>
#include <QLabel>
#include <QProgressBar>
#include <QGroupBox>
#include <QStackedWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QObject>

class DownloadMonitorPage : public QWidget {
    Q_OBJECT
 public:
  DownloadMonitorPage(QWidget* parent,
                      Session* session,
                      PreferenceManager* preference_manager,
                      const DownloadItem& params);
  std::pair<int, double> GetProgress();
  const DownloadItem& DBItem() { return db_item_; }
  void Resume();
  void Pause();
  void PauseAndClose();
  bool IsDone();
  bool IsPaused();
  void SetTabProgress();
  bool StartDownload();
  QString ValidateSaveAs(const QString& save_as,
                         const QString& suggested_save_as);
  void SetUpSpeedGrapher();
  void SetUpProgressStats();
  void SetUpShardBox();
  void SetUpNumConnectionsWidgets();
  void SetUpControlBox();

 signals:
  void RemoveTab(DownloadMonitorPage* page);
  void RefreshDownloadsTable();
  void SetTabProgress(DownloadMonitorPage* page, const QString& progress);

 public slots:
  void Cancel();
  void OnPaused();

 private slots:
  void ChangeNumConnections();
  void TogglePause();
  void OpenFile();
  void OpenParentDirectory();
  void UpdateSpeed();
  void SetDownloadedValueLabel(qint64 downloaded_bytes);
  void OnCompleted();
  void OnDownloadError(QNetworkReply::NetworkError code);
  void UpdateProgress();
  void DrawShardGrid();

 private:
  struct ShardRow {
   public:
    ShardRow(QLabel* label, QProgressBar* bar) : label_(label), bar_(bar) {}

    QLabel* Label() { return label_; }
    QProgressBar* ProgressBar() { return bar_; }

   private:
    QLabel* label_;
    QProgressBar* bar_;
  };

  DownloadMonitorPage(QWidget* parent, Session* session,
                      PreferenceManager* preference_manager);
  void Init();
  void OnCancelled();
  // int index();

  Session* session_;
  PreferenceManager* preference_manager_;
  StopWatch stop_watch_;
  SpeedGrapher* grapher_;
  DownloadItem db_item_;
  bool is_done_;
  double progress_;
  QString fname_;
  std::unique_ptr<Fetcher> fetcher_;
  QTimer speed_updater_;
  QTimer progress_updater_;
  qint64 prev_downloaded_bytes_;
  qint64 downloaded_bytes_;
  QLabel* downloaded_value_label_;
  QLabel* download_speed_value_label_;
  QGroupBox* shard_box_;
  std::vector<ShardRow> shard_rows_;
  QStackedWidget* button1_stack_;
  QStackedWidget* button2_stack_;
  QPushButton* close_button_;
  QLabel* num_connections_label_;
  QSpinBox* num_connections_spin_;
  QGroupBox* num_connections_box_;
  QLabel* download_speed_label_;
  QPushButton* pause_button_;
  QPushButton* num_connections_button_;
  QLabel* save_as_value_label_;
  bool non_resume_mode_;
  bool waiting_for_paused_;
  bool resume_on_paused_;
  bool cancel_on_paused_;
  bool close_on_paused_;
};

#endif  // DOWNLOAD_MONITOR_PAGE_H_
