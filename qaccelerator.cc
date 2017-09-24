// TODO(ogaro): Queued urls with same filename.
// TODO(ogaro): Use suggested filename in response headers.
// TODO(ogaro): Cancel paused?
// TODO(ogaro): Remember to place alert in DIE() macro.
// TODO(ogaro): Gracefully handle open when db instance already exists.
// TODO(ogaro): Show icon in downloads table while downloading.
// TODO(ogaro): Add method to resume by scanning downloaded segments.
// Add a method for cancel and close. Invoke when file size is exceeded.
// TODO(ogaro): Rename from flv to exe, zip in Windows is broken.
// TODO(ogaro): Ampersand in filename?
// TODO(ogaro): Possible race condition when writing to file.
// TODO(ogaro): Apostrophe in filename breaks things (Windows).

#include "qaccelerator.h"
#include "download-monitor.h"
#include "preferences-dialog.h"
#include "categorizer.h"
#include "version.h"

#include <QVBoxLayout>
#include <utility>
#include <QIcon>
#include <QToolBar>
#include <QSize>
#include <vector>
#include <QMenu>
#include <QMessageBox>
#include <QToolBar>
#include <QKeySequence>
#include <QMenuBar>
#include <QStatusBar>
#include <unordered_map>
#include <algorithm>
#include <QMessageBox>
#include "download-dialog.h"

using std::vector;
using std::pair;
using std::unordered_map;
typedef QMessageBox::StandardButton StandardButton;
typedef QMessageBox::StandardButtons StandardButtons;

const char* kStatusAll = "Status: All";
const char* kCategoryAll = "Category: All";

namespace {
void GetSubTreeNodes(QTreeWidgetItem* item,
                     vector<QTreeWidgetItem*>* all_items) {
  all_items->push_back(item);
  for (int i = 0; i < item->childCount(); ++i) {
    GetSubTreeNodes(item->child(i), all_items);
  }
}

void GetAllItems(QTreeWidget* widget, vector<QTreeWidgetItem*>* all_items) {
  for (int i = 0; i < widget->topLevelItemCount(); ++i) {
    GetSubTreeNodes(widget->topLevelItem(i), all_items);
  }
}

QStringList split(QString line, const QString& sep) {
  QStringList cells = line.split(
      sep,
      QString::SplitBehavior::KeepEmptyParts,
      Qt::CaseInsensitive);
  QStringList simplified;
  for (QString& cell : cells) {
    simplified.append(cell.toLower());
  }
  return simplified;
}

bool ParseImportFile(const QString& fpath,
                     vector<DownloadParams>* out,
                     QString* error) {
  QFile file(fpath);
  QString sep = "\t";
  if (file.open(QIODevice::ReadOnly)) {
    QTextStream stream(&file);
    QSet<QString> allowed_headers = {"url", "save as", "number of connections"};
    QStringList headers = split(stream.readLine(), sep); // Read header row.
    bool url_header_seen = false;
    for (const QString& header : headers) {
      if (!allowed_headers.contains(header)) {
        *error = "Unrecognized import file header " + header;
        return false;
      }
      if (header == "url") {
        url_header_seen = true;
      }
    }
    if (!url_header_seen) {
      *error = "Url header is required.";
      return false;
    }

    int line_number = 1;
    while(!stream.atEnd()) {
      line_number++;
      QStringList cells = split(stream.readLine(), sep);
      if (cells.size() != headers.size()) {
        *error = QString("Import row %1 has an incorrect number of cells. "
                         "Open the file in a text editor and make sure the "
                         "number of cells matches that of the header row.")
            .arg(line_number);
        return false;
      }
      for (int i = 0; i < cells.size(); ++i) {
        DownloadParams params; // Default ctor I have is not called. Weird..
        params.SetAccelerable(false);
        params.SetFileSize(0);
        params.SetNumConnections(0);
        const QString& header = headers[i];
        if (header == "url") {
          params.SetUrl(cells[i]);
          if (params.Url().isEmpty()) {
            *error = QString("Url is empty in row %d.").arg(line_number);
          }
        } else if (header == "save as") {
          params.SetSaveAs(cells[i]);
        } else if (header == "number of connections") {
          bool ok;
          params.SetNumConnections(QVariant(cells[i]).toInt(&ok));
          if (!ok) {
            *error = QString("Failed to parse number of connections "
                             "in line %d").arg(line_number);
            return false;
          }
        }
        out->push_back(params);
      }
    }
    file.close();
  } else {
    *error = "Failed to open import file.";
    return false;
  }
  return true;
}

}

MainWindow::MainWindow() {
  tree_widget_ = new QTreeWidget(this);
  qRegisterMetaType<FileSpec>();
  preference_manager_.reset(new PreferenceManager(&session_));
  downloads_table_ = new DownloadsTable(this);
  monitor_ = new DownloadMonitor(this, &session_, preference_manager_.get());
  download_dialog_ = new DownloadDialog(this, preference_manager_.get());
  QWidget* central_widget = new QWidget();
  setCentralWidget(central_widget);
  QVBoxLayout* layout = new QVBoxLayout(central_widget);
  //central_widget->setLayout(layout);
  tree_widget_->setColumnCount(1);

  // TODO(ogaro): Do operations like this in an initializer list.
  status_tree_item_ = new QTreeWidgetItem(tree_widget_);
  status_tree_item_->setText(0, kStatusAll);
  // TODO(ogaro): Add count in brackets for each filter.
  // TODO(ogaro): Add a function in qaccelerator-db that allows us to iterate through
  // these enums.
  statuses_ = QStringList({"In Progress", "Paused", "Completed", "Cancelled",
      "Failed", "Queued"});
  categories_ =  QStringList({"Videos", "Images", "Audio", "Documents",
      "Software", "Other"});
  foreach(const QString& status, statuses_) {
    QTreeWidgetItem* status_item = new QTreeWidgetItem(status_tree_item_);
    status_item->setText(0, status);
  }
  statuses_.append(kStatusAll);
  status_tree_item_->setExpanded(true);

  category_tree_item_ = new QTreeWidgetItem(tree_widget_);
  category_tree_item_->setText(0, kCategoryAll);
  foreach(const QString& category, categories_) {
    QTreeWidgetItem* category_item = new QTreeWidgetItem(category_tree_item_);
    category_item->setText(0, category);
  }
  categories_.append(kCategoryAll);
  category_tree_item_->setExpanded(true);

  prev_selected_items_ = tree_widget_->selectedItems();
  tree_widget_->setHeaderLabel("Filters");
  tree_widget_->setSelectionMode(QAbstractItemView::MultiSelection);
  progress_updater_ = new QTimer(this);
  connect(progress_updater_, SIGNAL(timeout()), this, SLOT(UpdateProgress()));

  // downloads_table_.reset(new DownloadsTable());
  // RefreshTable();
  splitter_.addWidget(tree_widget_);
  splitter_.addWidget(downloads_table_);
  splitter_.setStretchFactor(0, 1);
  splitter_.setStretchFactor(1, 4);
  layout->addWidget(&splitter_);
  CreateActions();
  CreateMenus();
  CreateToolbar();
  CreateStatusBar();

  // TODO(ogaro): Stop the progress updater when no downloads are in progress.
  // progress_updater_.start(progress_update_interval_ms_);

  downloads_table_->GenerateAndUpdateToolbarState();
  CreateConnections();
  status_tree_item_->setSelected(true);
  category_tree_item_->setSelected(true);
  RefreshTable();
  connect(tree_widget_, SIGNAL(itemSelectionChanged()),
          this, SLOT(OnTreeSelectionChanged()));
  setWindowTitle("QAccelerator");
  setWindowIcon(QIcon(":/images/qx_flash.png"));
  CleanUpFailedDownloads();
  showMaximized();
}

void MainWindow::CreateConnections() {
  connect(monitor_, SIGNAL(RefreshDownloadsTable()),
          this, SLOT(RefreshTable()));
  connect(monitor_, SIGNAL(AllTabsClosed()),
          this, SLOT(close()));
  connect(downloads_table_, &DownloadsTable::PauseDownload,
          [=] (DownloadItem item) {
    monitor_->PauseDownload(item);
  });
  connect(downloads_table_, &DownloadsTable::ResumeDownload,
          [=] (DownloadItem item) {
    monitor_->ResumeDownload(item);
  });
  connect(downloads_table_, &DownloadsTable::StartOrQueueDownload,
          [=] (DownloadItem item) {
    monitor_->StartOrQueueDownload(item);
  });
  connect(downloads_table_, &DownloadsTable::CancelDownload,
          [=] (DownloadItem item) {
    monitor_->CancelDownload(item);
  });
  connect(downloads_table_, &DownloadsTable::DeleteItem,
          [=] (DownloadItem item) {
    item.Delete();
    RefreshTable();
  });
  connect(downloads_table_, &DownloadsTable::UpdateToolbar,
          [=] (unordered_map<QString, bool> possibilities) {
    for (const auto& entry : possibilities) {
      actions_[entry.first]->setEnabled(entry.second);
    }
  });
  connect(downloads_table_, &DownloadsTable::Refresh, [=] {
    RefreshTable();
  });
  connect(downloads_table_, &DownloadsTable::CloneDownload,
          [=] (DownloadItem item) {
    Nullable<QString> save_as = item.SaveAs();
    QString url = item.Url().Get();
    DownloadParams params;

    // TODO(ogaro): Why the heck is this not initialized to zero?
    params.SetFileSize(0);

    params.SetUrl(url);
    if (!save_as.IsNull()) {
      params.SetSaveAs(save_as.Get());
    }
    params.SetNumConnections(item.NumConnections().Get());
    monitor_->AddDownload(params);
  });
  connect(download_dialog_, &DownloadDialog::accepted, [=] {
    if (download_dialog_->Params().Url().isEmpty()) {
      return;
    }
    monitor_->AddDownload(download_dialog_->Params());
  });
}

void MainWindow::CleanUpFailedDownloads() {
  vector<DownloadItem> items;
  DownloadItem::GetAll(&session_, &items);
  vector<DownloadItem> interrupted_items;
  bool table_refresh_needed = false;
  for (DownloadItem& item : items) {
    if (item.Status().Get() == DownloadItem::StatusEnum::IN_PROGRESS) {
      if (!item.WorkDir().IsNull()
          && QFileInfo(item.WorkDir().Get()).exists()) {
        interrupted_items.push_back(item);
      } else {
        table_refresh_needed = true;
        item.SetStatus(DownloadItem::StatusEnum::FAILED);
      }
    }
  }
  if (interrupted_items.empty()) {
    if (table_refresh_needed) {
      RefreshTable();
    }
    return;
  }
  /*StandardButton chosen =
      QMessageBox::question(
          this,
          "Resume interrupted downloads",
          "QAccelerator crashed while downloads were in progress. "
          "Would you like to resume them?",
          StandardButton::Ok | StandardButton::No,
          StandardButton::No);*/
  if (/*chosen == StandardButton::Ok */ false) {
    for (DownloadItem& item : interrupted_items) {
      Nullable<QString> work_dir = item.WorkDir();
      if (!work_dir.IsNull() && QFileInfo(work_dir.Get()).exists()) {
        QStringList fnames = QDir(work_dir.Get()).entryList(
            QDir::NoDotAndDotDot | QDir::Files);
        // TODO(ogaro): Parallelize file size checking.
        for(const QString& fname : fnames) {
          QFile shard(JoinPath(work_dir.Get(), fname));
          if (shard.open(QIODevice::ReadOnly)) {
            MaybeRenameShard(shard.size(), &shard);
            shard.close();
          } else {
            CHECK(shard.remove());
          }
        }
      }
      item.SetStatus(DownloadItem::StatusEnum::QUEUED);
    }
    // TODO(ogaro): The interrupted downloads are placed at the back of the
    // queue.
    monitor_->MaybePopQueueFront();
  } else {
    for (DownloadItem& item : interrupted_items) {
      Nullable<QString> work_dir = item.WorkDir();
      if (!work_dir.IsNull() && QFileInfo(work_dir.Get()).exists()) {
        QDir(work_dir.Get()).removeRecursively();
      }
      item.SetStatus(DownloadItem::StatusEnum::FAILED);
    }
  }
  RefreshTable();
}

void MainWindow::OnTreeSelectionChanged() {
  disconnect(tree_widget_, SIGNAL(itemSelectionChanged()), 0, 0);
  bool multiple_filters_mode;
  preference_manager_->Get("multiple_filters", &multiple_filters_mode);
  QTreeWidgetItem* item = nullptr;
  QList<QTreeWidgetItem*> currently_selected_items =
      tree_widget_->selectedItems();
  if (currently_selected_items.size() > prev_selected_items_.size()) {
    // Something selected.
    for (QTreeWidgetItem* curr_item : currently_selected_items) {
      if (std::find(prev_selected_items_.begin(),
                    prev_selected_items_.end(),
                    curr_item) == prev_selected_items_.end()) {
        item = curr_item;
        break;
      }
    }
  } else if (currently_selected_items.size() < prev_selected_items_.size()) {
    // Something deselected.
    for (QTreeWidgetItem* curr_item : prev_selected_items_) {
      if (std::find(currently_selected_items.begin(),
                    currently_selected_items.end(),
                    curr_item) != currently_selected_items.end()) {
        item = curr_item;
        break;
      }
    }
  }
  if (multiple_filters_mode) {
    SanitizeTreeSelectionMultiMode(item);
  } else {
    SanitizeTreeSelectionSingleMode(item);
  }
  prev_selected_items_ = tree_widget_->selectedItems();
  RefreshTable();
  connect(tree_widget_, SIGNAL(itemSelectionChanged()),
          this, SLOT(OnTreeSelectionChanged()));
}

void MainWindow::SanitizeTreeSelectionSingleMode(QTreeWidgetItem* item) {
  if (item == nullptr) {
    // ResetTreeSelection(false);
    return;
  } else if (!item->isSelected()) {
    qDebug() << "Item is not selected " << item->text(0);
    item->setSelected(true);
    return;
  }
  QTreeWidgetItem* prev_status_item = nullptr;
  QTreeWidgetItem* prev_category_item = nullptr;
  for (QTreeWidgetItem* prev_item : prev_selected_items_) {
    if (statuses_.contains(prev_item->text(0))) {
      prev_status_item = prev_item;
    } else if (categories_.contains(prev_item->text(0))) {
      prev_category_item = prev_item;
    }
  }
  if (prev_status_item == nullptr || prev_category_item == nullptr) {
    ResetTreeSelection(false);
    return;
  }
  if (statuses_.contains(item->text(0))) {
    prev_status_item->setSelected(false);
  }
  if (categories_.contains(item->text(0))) {
    prev_category_item->setSelected(false);
  }

  // At this point, if more or less than one status or category is selected,
  // reset the selection.
  int num_selected_statuses = 0;
  int num_selected_categories = 0;
  vector<QTreeWidgetItem*> all_items;
  GetAllItems(tree_widget_, &all_items);
  for (QTreeWidgetItem* some_item : all_items) {
    if (!some_item->isSelected()) {
      continue;
    }
    if (statuses_.contains(some_item->text(0))) {
      num_selected_statuses++;
    } else if (categories_.contains(some_item->text(0))) {
      num_selected_categories++;
    }
  }
  if (num_selected_categories != 1 || num_selected_statuses != 1) {
    ResetTreeSelection(false);
  }
}

void MainWindow::SanitizeTreeSelectionMultiMode(QTreeWidgetItem* item) {
  disconnect(tree_widget_, SIGNAL(itemSelectionChanged()), 0, 0);
}

void MainWindow::ResetTreeSelection(bool disable_connection) {
  bool multiple_filters;
  preference_manager_->Get("multiple_filters", &multiple_filters);
  if (multiple_filters) {
    return;
  }
  if (disable_connection) {
    disconnect(tree_widget_, SIGNAL(itemSelectionChanged()), 0, 0);
  }
  vector<QTreeWidgetItem*> all_items;
  GetAllItems(tree_widget_, &all_items);
  for (QTreeWidgetItem* item : all_items) {
    if (item->text(0) == kStatusAll) {
      item->setSelected(true);
    } else if (item->text(0) == kCategoryAll) {
      item->setSelected(true);
    } else {
      item->setSelected(false);
    }
  }
  prev_selected_items_ = tree_widget_->selectedItems();
  if (disable_connection) {
    connect(tree_widget_, SIGNAL(itemSelectionChanged()),
            this, SLOT(OnTreeSelectionChanged()));
  }
}

void MainWindow::RefreshTable() {
  if (downloads_table_->RenameInProgress()
      || downloads_table_->BatchOperationInProgress()) {
    return;
  }
  QList<QTreeWidgetItem*> selected_items = tree_widget_->selectedItems();
  QString status;
  QString category;
  for (QTreeWidgetItem* item : selected_items) {
    if (statuses_.contains(item->text(0))) {
      status = item->text(0);
    } else if (categories_.contains(item->text(0))) {
      category = item->text(0);
    }
  }
  if (status.isEmpty() || category.isEmpty()) {
    qDebug() << "At least one of status or category is not selected.";
    return;
  }
  downloads_table_->Reset();
  vector<DownloadItem> items;
  DownloadItem::GetAll(&session_, &items);
  bool download_in_progress = false;
  for (DownloadItem& item : items) {
    Category item_category = Category::OTHER;
    Nullable<QString> save_as = item.SaveAs();
    if (!save_as.IsNull() && !save_as.Get().isEmpty()) {
      item_category = Categorizer::Categorize(FileExt(item.SaveAs().Get()));
    }
    QString item_status = DownloadItem::ToString(item.Status().Get());
    if ((status == kStatusAll || status == item_status)
        && (category == kCategoryAll
            || item_category == StringToCategory(category))) {
      downloads_table_->AddRow(item);
    }
    download_in_progress |= (
        item.Status().Get() == DownloadItem::StatusEnum::IN_PROGRESS);
  }
  if (download_in_progress && !progress_updater_->isActive()) {
    progress_updater_->start(kProgressUpdateInterval);
  } else if (!download_in_progress && progress_updater_->isActive()) {
    progress_updater_->stop();
  }
}

// TODO(ogaro): There is probably a better way of boilerplating creation of
// actions.
// TODO(ogaro): Rename those damn images ('play.png' to 'resume.png');
void MainWindow::CreateActions() {
  actions_["add_new_download"] = (new QAction(
      QIcon(":images/plus.png"), "Add new", this));
  actions_["add_new_download"]->setShortcut(QKeySequence("Ctrl+N"));
  actions_["add_new_download"]->setStatusTip("Start new download");
  connect(actions_["add_new_download"], SIGNAL(triggered()),
          this, SLOT(AddNewDownload()));

  actions_["import_downloads"] = (new QAction(
      QIcon(":images/import.png"), "Import from file...", this));
  actions_["import_downloads"]->setShortcut(QKeySequence("Ctrl+I"));
  actions_["import_downloads"]->setStatusTip("Import download list");
  connect(actions_["import_downloads"], SIGNAL(triggered()),
          this, SLOT(ImportDownloads()));

  actions_["pause"] = (new QAction(
      QIcon(":images/pause.png"), "Pause", this));
  actions_["pause"]->setStatusTip("Pause download(s)");
  connect(actions_["pause"], &QAction::triggered, [=] {
    downloads_table_->PerformActionOnSelected("pause");
  });

  actions_["resume"] = (new QAction(
      QIcon(":images/play.png"), "Resume", this));
  actions_["resume"]->setStatusTip("Resume download(s)");
  connect(actions_["resume"], &QAction::triggered, [=] {
    downloads_table_->PerformActionOnSelected("resume");
  });

  actions_["pause"] = (new QAction(
      QIcon(":images/pause.png"), "Pause", this));
  actions_["pause"]->setStatusTip("Pause download(s)");
  connect(actions_["pause"], &QAction::triggered, [=] {
    downloads_table_->PerformActionOnSelected("pause");
  });

  actions_["cancel"] = (new QAction(
      QIcon(":images/multiply.png"), "Cancel", this));
  actions_["cancel"]->setStatusTip("Cancel download(s)");
  connect(actions_["cancel"], &QAction::triggered, [=] {
    downloads_table_->PerformActionOnSelected("cancel");
  });

  actions_["clone"] = (new QAction(
      QIcon(":images/refresh.png"), "Clone", this));
  actions_["clone"]->setStatusTip("Clone and restart download(s)");
  connect(actions_["clone"], &QAction::triggered, [=] {
    downloads_table_->PerformActionOnSelected("clone");
  });

  actions_["delete_entry"] = (new QAction("From history", this));
  connect(actions_["delete_entry"], &QAction::triggered, [=] {
    downloads_table_->PerformActionOnSelected("delete_entry");
  });

  actions_["delete_file"] = (new QAction("From history and disk", this));
  connect(actions_["delete_file"], &QAction::triggered, [=] {
    downloads_table_->PerformActionOnSelected("delete_file");
  });

  actions_["change_preferences"] = (new QAction(
      QIcon(":images/gear.png"), "Preferences...", this));
  actions_["change_preferences"]->setStatusTip("Change preferences");
  connect(actions_["change_preferences"], SIGNAL(triggered()),
          this, SLOT(PromptPreferences()));

  actions_["exit"] = (new QAction("E&xit", this));
  actions_["exit"]->setShortcut(QKeySequence("Ctrl+Q"));
  actions_["exit"]->setStatusTip("Exit the application");
  connect(actions_["exit"], SIGNAL(triggered()),
          this, SLOT(close()));

  actions_["about"] = (new QAction("About QAccelerator...", this));
  connect(actions_["about"], &QAction::triggered, [=] {
    QChar copyright[] = {0x00A9};
    QString about_message =
        QString("Version %1 <br><br>"
                "<a href='http://ogaro.io/qaccelerator'>http://ogaro.io/qaccelerator</a><br><br>"
                "Copyright %2 %3 Denver Ogaro<br>"
                "denver@ogaro.net")
          .arg(VERSION)
          .arg(QString::fromRawData(copyright, 1))
          .arg(COPYRIGHT_YEAR);
    QMessageBox mb(
        "QAccelerator",
         about_message,
         QMessageBox::Information,
         QMessageBox::NoButton,
         QMessageBox::NoButton,
         QMessageBox::NoButton);
    mb.setTextFormat(Qt::TextFormat::RichText);
    mb.exec();
  });
}

void MainWindow::CreateMenus() {
  QMenu* file_menu = menuBar()->addMenu("&File");
  file_menu->addAction(actions_["exit"]);
  QMenu* downloads_menu = menuBar()->addMenu("&Downloads");
  downloads_menu->addActions({
      actions_["add_new_download"],
      actions_["import_downloads"]
  });
  delete_menu_.addActions({
       actions_["delete_entry"],
       actions_["delete_file"]
  });
  downloads_menu->addSeparator();
  QMenu* help_menu = menuBar()->addMenu("&Help");
  help_menu->addAction(actions_["about"]);
}

void MainWindow::CreateToolbar() {
  QToolBar* file_toolbar = addToolBar("File");
  file_toolbar->setIconSize(QSize(48, 48));
  file_toolbar->addActions({
      actions_["add_new_download"],
      actions_["pause"],
      actions_["resume"],
      actions_["cancel"],
      actions_["clone"]
  });
  delete_button_.setMenu(&delete_menu_);
  delete_button_.setPopupMode(QToolButton::InstantPopup);
  delete_button_.setIcon(QIcon(":images/bin.png"));
  delete_button_.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  delete_button_.setText("Delete");
  file_toolbar->addWidget(&delete_button_);
  file_toolbar->addActions({
      actions_["import_downloads"],
      actions_["change_preferences"]
  });
  file_toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
}

void MainWindow::CreateStatusBar() {
  statusBar()->showMessage("Ready");
}

void MainWindow::UpdateProgress() {
  vector<pair<int, double> > progress_tuples;
  monitor_->GetProgress(&progress_tuples);
  for (const auto& tup : progress_tuples) {
    downloads_table_->SetProgress(tup.first, tup.second);
  }
}

void MainWindow::AddNewDownload() {
  download_dialog_->ResetFields();
  download_dialog_->CopyUrlFromClipboard();
  download_dialog_->show();
  /*
  if (params.Url().isEmpty()) {
    return;
  }
  monitor_->AddDownload(params);
  monitor_->show();*/
}

void MainWindow::ImportDownloads() {
  QString download_dir;
  preference_manager_->Get("download_dir", &download_dir);
  QString selected_filter;
  QString download_path = QFileDialog::getOpenFileName(
      this,
      "Import downloads",
      download_dir,
      "*.txt;;*.csv;;*",
      &selected_filter);
  if (download_path.isEmpty()) {
    return;
  }
  vector<DownloadParams> parsed;
  QString error;
  if (!ParseImportFile(download_path, &parsed, &error)) {
    QMessageBox::critical(this,
                          "Invalid import file",
                          error);
    return;
  }
  for (DownloadParams& params : parsed) {
    monitor_->QueueDownload(params);
  }
  monitor_->MaybePopQueueFront();
}

void MainWindow::PromptPreferences() {
  (new PreferencesDialog(this, preference_manager_.get()))->exec();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (monitor_->count() < 1) {
    event->accept();
  } else {
    hide();
    monitor_->close();
    event->ignore();
  }
}
