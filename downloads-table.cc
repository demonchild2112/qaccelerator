#include "downloads-table.h"

#include "qaccelerator-utils.h"
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QFile>
#include <QTableWidgetItem>
#include <unordered_set>
#include <QAction>
#include <utility>
#include <QMenu>
#include <memory>
#include <QApplication>
#include <QClipboard>

using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::pair;
using std::make_pair;
using std::unique_ptr;
typedef DownloadItem::StatusEnum Status;

static const char* ICON_FILES_DIRNAME = "icon_files";

namespace {
QString ProgressToString(double progress) {
  return QString::number(progress * 100, 'f', 1) + "%";
}
}

DownloadsTable::DownloadsTable(QWidget* parent)
    : QTableWidget(parent) {
  rename_in_progress_ = false;
  batch_operation_in_progress_ = false;
  displayed_field_names_ = QStringList({
      "File",
      "Size",
      "Status",
      "Progress",
      "Start Time"});
  setColumnCount(displayed_field_names_.size());
  setHorizontalHeaderLabels(displayed_field_names_);
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  foreach(const QString& field_name, displayed_field_names_) {
    normalized_field_names_.append(NormalizeFieldName(field_name));
  }
  connect(this, SIGNAL(cellChanged(int, int)),
          this, SLOT(OnCellChanged(int, int)));
  connect(this, SIGNAL(cellDoubleClicked(int, int)),
          this, SLOT(OnCellDoubleClicked(int, int)));
}

DownloadsTable::~DownloadsTable() {}

void DownloadsTable::selectionChanged(const QItemSelection& selected,
                                 const QItemSelection& deselected) {
  QTableWidget::selectionChanged(selected, deselected);
  GenerateAndUpdateToolbarState();
}

QString DownloadsTable::NormalizeFieldName(const QString& field_name) {
  return field_name.toLower().replace(' ', '_');
}

void DownloadsTable::OpenFile(const QString& fpath) {
  QDesktopServices::openUrl(QUrl::fromLocalFile(fpath));
}

void DownloadsTable::OpenFolder(const QString& fpath) {
  QDesktopServices::openUrl(QUrl::fromLocalFile(DirName(fpath)));
}

void DownloadsTable::DeleteFile(DownloadItem& item) {
  QFile file(item.SaveAs().Get());
  if (file.exists()) {
    file.remove();
  }
  emit DeleteItem(item);
}

void DownloadsTable::OnCellDoubleClicked(int row, int column) {
  DownloadItem item = row_to_db_item_.find(row).value();
  Nullable<QString> save_as = item.SaveAs();
  if (!save_as.IsNull() && QFile(save_as.Get()).exists()) {
    OpenFile(save_as.Get());
  }
}

void DownloadsTable::OnCellChanged(int row, int column) {
  if (!rename_in_progress_) {
    return;
  }
  disconnect(this, SIGNAL(cellChanged(int, int)), 0, 0);
  DownloadItem db_item = row_to_db_item_.find(row).value();
  QTableWidgetItem* name_item = item(row, column);
  QString db_save_as;
  Nullable<QString> raw = db_item.SaveAs();
  if (!raw.IsNull()) {
    db_save_as = raw.Get();
  }
  QString db_fname = QFileInfo(db_save_as).fileName();
  QString new_fname = name_item->text();
  bool rename_op_done = false;
  QString new_save_as;

  if (db_fname != new_fname && new_fname != "") {
    QString download_dir = DirName(db_save_as);
    new_save_as = JoinPath(download_dir, new_fname);
    rename_op_done = QFile::rename(db_save_as, new_save_as);
  }
  rename_in_progress_ = false;
  // emit RenameFinished(db_item, new_save_as, rename_op_done);
  if (new_fname != db_fname && !rename_op_done) {
    name_item->setText(db_fname);
  } else {
    db_item.SetSaveAs(new_save_as);
  }
  name_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
  name_item->setIcon(GetIcon(name_item->text()));
  connect(this, SIGNAL(cellChanged(int, int)),
          this, SLOT(OnCellChanged(int, int)));
}

void DownloadsTable::Rename(int row) {
  disconnect(this, SIGNAL(cellChanged(int, int)), 0, 0);
  rename_in_progress_ = true;
  QTableWidgetItem* name_item = item(row, 0);
  setCurrentItem(name_item);
  name_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                      Qt::ItemIsEditable);
  editItem(name_item);
  connect(this, SIGNAL(cellChanged(int, int)),
          this, SLOT(OnCellChanged(int, int)));
}

void DownloadsTable::GenerateAndUpdateToolbarState() {
  unordered_map<QString, bool> possibilities = {
    {"pause", false},
    {"cancel", false},
    {"resume", false},
    {"delete_file", false},
    {"delete_entry", false},
    {"clone", false}
  };
  if (selectionModel()->selectedIndexes().isEmpty()) {
    emit UpdateToolbar(possibilities);
    return;
  }
  for (const auto& entry : possibilities) {
    possibilities[entry.first] = true;
  }
  for (const QModelIndex& index : selectionModel()->selectedIndexes()) {
    // TODO(ogaro): Remove once we can use const downloaditem everywhere.
    DownloadItem item = const_cast<DownloadItem&>(
        row_to_db_item_.find(index.row()).value());
    for (const auto& entry : possibilities) {
      possibilities[entry.first] &= ActionIsPossible(entry.first, item);
    }
  }
  emit UpdateToolbar(possibilities);
}

bool DownloadsTable::ActionIsPossible(const QString& action,
                                      DownloadItem& item) {
  // TODO(ogaro): Use enum instead of string as action.
  // TODO(ogaro): Avoid double database lookups when checking for nullness.
  // qDebug() << "Checking status for item" << item.Id();
  // Nullable<Status> status = item.Status();
  Nullable<DownloadItem::StatusEnum> status = item.Status();
  Nullable<QString> save_as = item.SaveAs();
  if (action == "pause") {
    return status.Get() == Status::IN_PROGRESS;
  } else if (action == "cancel") {
    return status.Get() == Status::IN_PROGRESS ||
        status.Get() == Status::PAUSED;
  } else if (action == "resume") {
    return (status.Get() == Status::PAUSED || status.Get() == Status::QUEUED);
  } else if (action == "open") {
    return !save_as.IsNull() && QFile::exists(save_as.Get());
  } else if (action == "open_folder") {
    return !save_as.IsNull() &&
        QFile::exists(DirName(save_as.Get()));
  } else if (action == "clone") {
    return status.Get() == Status::COMPLETED
        || status.Get() == Status::CANCELLED
        || status.Get() == Status::FAILED;
  } else if (action == "delete_entry") {
    return true;
    // return item.Status().Get() != Status::IN_PROGRESS;
  } else if (action == "delete_file") {
    return status.Get() == Status::COMPLETED &&
        !save_as.IsNull() && QFile::exists(save_as.Get());
  } else if (action == "rename") {
    return status.Get() == Status::COMPLETED &&
        !save_as.IsNull() && QFile::exists(save_as.Get());
  } else if (action == "copy_url") {
    return !item.Url().IsNull();
  } else {
    DIE() << "Action '" << action << "' not recognized.";
  }
  return false;
}

void DownloadsTable::PerformAction(const QString& action,
                                   DownloadItem& item) {
  // TODO(ogaro): Use enum instead of string as action.
  if (action == "pause") {
    emit PauseDownload(item);
  } else if (action == "cancel") {
    emit CancelDownload(item);
  } else if (action == "resume") {
    if (item.Status().Get() == DownloadItem::StatusEnum::QUEUED) {
      emit StartOrQueueDownload(item);
    } else {
      emit ResumeDownload(item);
    }
  } else if (action == "open") {
    OpenFile(item.SaveAs().Get());
  } else if (action == "open_folder") {
    OpenFolder(item.SaveAs().Get());
  } else if (action == "clone") {
    emit CloneDownload(item);
  } else if (action == "delete_entry") {
    if (item.Status().Get() == Status::IN_PROGRESS
        || item.Status().Get() == Status::PAUSED) {
      emit CancelDownload(item);
    }
    emit DeleteItem(item);
  } else if (action == "delete_file") {
    DeleteFile(item);
  } else if (action == "rename") {
    // TODO(ogaro): Find or null
    int row = db_id_to_row_.find(item.Id()).value();
    Rename(row);
  } else if (action == "copy_url") {
    QApplication::clipboard()->setText(item.Url().Get());
  } else {
    DIE() << "Action '" << action << "' not recognized.";
  }
}

void DownloadsTable::PerformActionOnSelected(const QString& action) {
  batch_operation_in_progress_ = true;
  unordered_set<int> selected_rows;
  for (const QModelIndex& index : selectionModel()->selectedIndexes()) {
    selected_rows.insert(index.row());
  }
  for (int row : selected_rows) {
    DownloadItem& item = row_to_db_item_.find(row).value();
    CHECK(item.IsValid());
    if (ActionIsPossible(action, item)) {
      PerformAction(action, item);
    }
  }
  batch_operation_in_progress_ = false;
  emit Refresh();
}

// TODO(ogaro): Delete all temporary actions when exec returns.
void DownloadsTable::ShowPopup(QMouseEvent* event) {
  QModelIndexList indexes = selectionModel()->selectedIndexes();
  if (indexes.isEmpty()) {
    return;
  }
  int row = indexes[0].row();
  DownloadItem& item = row_to_db_item_.find(row).value();
  vector<pair<QString, QAction*> > actions;
  actions.push_back(make_pair("pause",
      new QAction(QIcon(":/images/pause.png"), "Pause", this)));
  actions.push_back(make_pair("cancel",
      new QAction(QIcon(":/images/multiply.png"), "Cancel", this)));
  actions.push_back(make_pair("resume",
      new QAction(QIcon(":/images/play.png"), "Resume", this)));
  actions.push_back(make_pair("open",
      new QAction(QIcon(":/images/right_arrow.png"), "Open", this)));
  actions.push_back(make_pair("open_folder",
      new QAction(QIcon(":/images/folder.png"), "Open folder", this)));
  actions.push_back(make_pair("clone",
      new QAction(QIcon(":/images/refresh.png"), "Clone and restart", this)));

  QMenu delete_menu("Delete");
  QAction* delete_entry_act = new QAction("From history", this);
  QAction* delete_file_act = new QAction("From history and disk", this);
  actions.push_back(make_pair("delete_entry", delete_entry_act));
  actions.push_back(make_pair("delete_file", delete_file_act));
  delete_menu.addAction(delete_entry_act);
  delete_menu.addAction(delete_file_act);
  actions.push_back(make_pair("rename", new QAction("Rename", this)));
  actions.push_back(make_pair("copy_url",
                              new QAction("Copy url to clipboard", this)));

  QMenu menu;
  for (pair<QString, QAction*>& entry : actions) {
    const QString& action_name = entry.first;
    QAction& action = *(entry.second);
    bool enable_action = ActionIsPossible(action_name, item);
    action.setEnabled(enable_action);
    action.setIconVisibleInMenu(true);
    if (enable_action) {
      connect(&action, &QAction::triggered, [=] () mutable {
        this->PerformAction(action_name, item);
      });
    }
    if (action_name == "delete_entry") {
      continue;
    } else if (action_name == "delete_file") {
      menu.addMenu(&delete_menu);
      continue;
    }
    menu.addAction(&action);
  }
  menu.exec(event->globalPos());
}

void DownloadsTable::mousePressEvent(QMouseEvent* event) {
  QTableWidget::mousePressEvent(event);
  if (event->button() == Qt::RightButton) {
    ShowPopup(event);
  }
}

void DownloadsTable::SetProgress(int db_item_id,
                                 double progress) {
  auto row = db_id_to_row_.find(db_item_id);
  if (row == db_id_to_row_.end()) {
    return; // Item does not match currently selected status and category.
  }
  int col = normalized_field_names_.indexOf("progress");
  item(row.value(), col)->setText(ProgressToString(progress));
}

QStringList DownloadsTable::ParseFieldsFromDBItem(DownloadItem& item) {
  QString file_size_str = "";
  QString progress_str = "";
  QString file = "";
  QString start_time_str = "";

  Nullable<qint64> file_size = item.FileSize();
  if (!file_size.IsNull()) {
    if (file_size.Get() > 0) {
      file_size_str = IntFileSizeToString(file_size.Get());
    } else {
      file_size_str = "Unknown";
    }
  }
  Nullable<qint64> start_time = item.StartTime();
  if (!start_time.IsNull()) {
    if (start_time.Get() > 0) {
      start_time_str = StringifyTimeStamp(start_time.Get());
    } else {
      start_time_str = "Not started";
    }
  }
  if (item.Status().Get() == Status::COMPLETED) {
    progress_str = "100.0%";
  } else {
    Nullable<double> progress = item.Progress();
    if (!progress.IsNull()) {
      progress_str = ProgressToString(progress.Get());
    }
  }
  Nullable<QString> save_as = item.SaveAs();
  if (!save_as.IsNull()) {
    file = FileName(save_as.Get());
  }
  // In order of displayed_field_names_;
  QString status_str = DownloadItem::ToString(item.Status().Get());
  return QStringList({
      file,
      file_size_str,
      status_str,
      progress_str,
      start_time_str});
}

void DownloadsTable::AddRow(DownloadItem& db_item) {
  disconnect(this, SIGNAL(cellChanged(int, int)), 0, 0);
  int last_row_id = rowCount();
  insertRow(last_row_id);

  QStringList row_contents = ParseFieldsFromDBItem(db_item);
  for (int i = 0; i < normalized_field_names_.size(); ++i) {
    QTableWidgetItem* item = new QTableWidgetItem(row_contents[i]);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    if (i == 0) {
      Nullable<QString> save_as = db_item.SaveAs();
      if (!save_as.IsNull()) {
        item->setIcon(GetIcon(save_as.Get()));
      }
    }
    setItem(last_row_id, i, item);
  }
  resizeColumnsToContents();
  db_id_to_row_[db_item.Id()] = last_row_id;
  row_to_db_item_[last_row_id] = DownloadItem(db_item);
  connect(this, SIGNAL(cellChanged(int, int)),
          this, SLOT(OnCellChanged(int, int)));
}

const QIcon& DownloadsTable::GetIcon(const QString& fpath) {
  QString file_ext = FileExt(fpath);
  auto cached = file_ext_to_icon_.find(file_ext);
  if (cached != file_ext_to_icon_.end()) {
    return cached.value();
  }
  unique_ptr<QIcon> icon;
  if (QFile::exists(fpath)) {
    icon.reset(new QIcon(QFileIconProvider().icon(QFileInfo(fpath))));
  } else {
    QString icon_files_dir = JoinPath(QDir::currentPath(), ICON_FILES_DIRNAME);
    if (!QFile::exists(icon_files_dir)) {
      QDir(QDir::currentPath()).mkdir(ICON_FILES_DIRNAME);
    }
    QString temp_fpath = JoinPath(icon_files_dir,
                                  "qaccelerator_temp." + file_ext);
    QFile temp_file(temp_fpath);
    if (!temp_file.exists()) {
      temp_file.open(QFile::WriteOnly);
      temp_file.close();
    }
    icon.reset(new QIcon(QFileIconProvider().icon(QFileInfo(temp_file))));
  }
  if (icon->isNull()) {
    icon.reset(new QIcon(":images/default_file_icon.png"));
  }
  file_ext_to_icon_[file_ext] = *icon;
  return file_ext_to_icon_[file_ext];
}

void DownloadsTable::Reset() {
  setRowCount(0);
  db_id_to_row_.clear();
  row_to_db_item_.clear();
}
