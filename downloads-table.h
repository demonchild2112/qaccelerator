#ifndef DOWNLOADS_TABLE_H_
#define DOWNLOADS_TABLE_H_

#include "qaccelerator-utils.h"
#include "qaccelerator-db.h"
#include <QTableWidget>
#include <unordered_map>
#include <vector>
#include <memory>
#include <QStringList>
#include <QIcon>
#include <QFileIconProvider>
#include <QMouseEvent>

// TODO(ogaro): Rename item to db_item everywhere.
// TODO(ogaro): Use const DownloadItem& instead of DownloadItem&
class DownloadsTable : public QTableWidget {
  Q_OBJECT

 public:
  DownloadsTable(QWidget* parent);
  ~DownloadsTable();

  void OpenFile(const QString& fpath);
  void OpenFolder(const QString& fpath);
  void DeleteFile(DownloadItem& item);
  bool RenameInProgress() {
    return rename_in_progress_;
  }
  void GenerateAndUpdateToolbarState();
  void Reset();
  void AddRow(DownloadItem& item);
  void PerformActionOnSelected(const QString& action_name);
  void SetProgress(int db_item_id, double progress);
  bool BatchOperationInProgress() { return batch_operation_in_progress_; }

 signals:
  void ResumeDownload(DownloadItem& item);
  void PauseDownload(DownloadItem& item);
  void CancelDownload(DownloadItem& item);
  void CloneDownload(DownloadItem& item);
  void DeleteItem(DownloadItem& item);
  void StartOrQueueDownload(DownloadItem& item);
  void UpdateToolbar(const std::unordered_map<QString, bool>& possibilities);
  void Refresh();
  //void RenameFinished(DownloadItem& item, const QString& new_save_as,
  //                    bool rename_op_done);

 protected:
  // Override
  void selectionChanged(const QItemSelection& selected,
                        const QItemSelection& deselected);

  // Override
  void mousePressEvent(QMouseEvent * event);

 // TODO(ogaro): Move all slots to this section.
 private slots:
  void OnCellChanged(int row, int column);
  void OnCellDoubleClicked(int row, int column);
  void Rename(int row);

 private:
  QString NormalizeFieldName(const QString& field_name);
  const QIcon& GetIcon(const QString& fpath);
  bool ActionIsPossible(const QString& action_name,
                        DownloadItem& item);
  void PerformAction(const QString& action_name,
                     DownloadItem& item);
  void ShowPopup(QMouseEvent* event);
  QStringList ParseFieldsFromDBItem(DownloadItem& item);

  QStringList displayed_field_names_;
  QStringList normalized_field_names_;
  // TODO(ogaro): Do something more efficient than copying the DownloadItem
  // object around?
  QMap<int, int> db_id_to_row_;
  QMap<int, DownloadItem> row_to_db_item_;
  QMap<QString, QIcon> file_ext_to_icon_;
  bool rename_in_progress_;
  bool batch_operation_in_progress_;
};

#endif // DOWNLOADS_TABLE_H_
