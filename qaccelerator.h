#ifndef QACCELERATOR_H
#define QACCELERATOR_H

// TODO(ogaro): Do not release repo as is to the public.
// TODO(ogaro): Pass in parent for all qobjects

#include "qaccelerator-db.h"
#include "downloads-table.h"
#include "download-monitor.h"
#include "download-dialog.h"

#include <memory>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTimer>
#include <QSplitter>
#include <unordered_map>
#include <QMenu>
#include <QToolButton>
#include <QClipboard>

class MainWindow : public QMainWindow {
    Q_OBJECT
 public:
  MainWindow();
  ~MainWindow() {}

protected:
 void closeEvent(QCloseEvent* event);

 private slots:
  void OnTreeSelectionChanged();
  void UpdateProgress();
  void AddNewDownload();
  void ImportDownloads();
  void PromptPreferences();
  void RefreshTable();

 private:
  void CleanUpFailedDownloads();
  void CreateActions();
  void CreateMenus();
  void CreateToolbar();
  void CreateStatusBar();
  void SanitizeTreeSelectionMultiMode(QTreeWidgetItem* item);
  void SanitizeTreeSelectionSingleMode(QTreeWidgetItem* item);
  void ResetTreeSelection(bool disable_connection);
  void CreateConnections();

  Session session_;
  bool multiple_filters_mode_;
  std::unique_ptr<PreferenceManager> preference_manager_;

  QTreeWidget* tree_widget_;
  QTreeWidgetItem* status_tree_item_;
  QTreeWidgetItem* category_tree_item_;

  QList<QTreeWidgetItem*> prev_selected_items_;
  bool waiting_for_renamed_;
  QTimer* progress_updater_;
  // std::unique_ptr<DownloadsTable> downloads_table_;
  DownloadsTable* downloads_table_;
  QSplitter splitter_;
  std::unordered_map<QString, QAction*> actions_;
  QMenu delete_menu_;
  QToolButton delete_button_;

  // TODO(ogaro): These should live in qaccelerator-db
  QStringList statuses_;
  QStringList categories_;

  DownloadMonitor* monitor_;
  DownloadDialog* download_dialog_;
};

#endif // QACCELERATOR_H
