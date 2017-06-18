#include "download-dialog.h"

#include <QSizePolicy>
#include <QVBoxLayout>
#include <QClipboard>
#include <QMessageBox>
#include <QFile>
#include <QApplication>
#include <QToolTip>
#include <QPoint>

// TODO(ogaro): Set file type when save as changes.
// TODO(ogaro): Correctly handle extensions like tar.gz
// TODO(ogaro): Sanitize file names.
// TODO(ogaro): Find out what happens when dialog closes without clicking button.
DownloadDialog::DownloadDialog(QWidget* parent,
                               PreferenceManager* preference_manager)
    : QDialog(parent) {
  preference_manager_ = preference_manager;
  current_getter_id_ = -1;
  SetUpFileSpecWidgets();
  QIcon download_icon(":/images/download.png");
  download_button_ = new QPushButton(download_icon, "Download", this);
  download_button_->setDefault(true);
  download_button_->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,
                                              QSizePolicy::Fixed));
  connect(download_button_, SIGNAL(clicked()), this, SLOT(LaunchDownload()));
  SetUpConfigWidgets();
  divider_ = new QFrame();
  divider_->setFrameShape(QFrame::HLine);
  divider_->setFrameShadow(QFrame::Sunken);
  setObjectName("QX");
  clipboard_ = QApplication::clipboard();
  QVBoxLayout* layout = new QVBoxLayout();
  layout->addWidget(file_spec_gbox_);
  layout->addWidget(download_button_);
  layout->addWidget(divider_);
  layout->addWidget(config_gbox_);
  setLayout(layout);
  setWindowTitle("QAccelerator");
  setWindowIcon(QIcon(":/images/qx_flash.png"));
}

DownloadDialog::~DownloadDialog() {}

void DownloadDialog::ResetFields() {
  url_edit_->setText("");
  url_edit_->setStyleSheet("");
  url_edit_->setToolTip("");
  save_as_edit_->setText("");
  num_connections_sbox_->setEnabled(true);
  int default_num_connections;
  preference_manager_->Get("num_connections", &default_num_connections);
  num_connections_sbox_->setValue(default_num_connections);
}

void DownloadDialog::SetUpFileSpecWidgets() {
  file_spec_gbox_ = new QGroupBox("File Spec", this);
  QGridLayout* file_spec_layout = new QGridLayout();
  file_spec_gbox_->setLayout(file_spec_layout);

  // First row: url
  url_label_ = new QLabel("Url:", this);
  file_spec_layout->addWidget(url_label_, 0, 0);
  url_edit_ = new QLineEdit(this);
  url_edit_->setMinimumWidth(500);
  url_edit_->setPlaceholderText("Enter url");
  url_edit_->setStyleSheet("");
  connect(url_edit_, SIGNAL(textChanged(QString)),
          this, SLOT(GetFileSpec(QString)));
  file_spec_layout->addWidget(url_edit_, 0, 1);

  // Second row: save as
  save_as_label_ = new QLabel("Save as:", this);
  file_spec_layout->addWidget(save_as_label_);
  save_as_edit_ = new QLineEdit(this);
  save_as_edit_->setPlaceholderText("Destination file");
  file_spec_layout->addWidget(save_as_edit_, 1, 1);
  save_as_button_ = new QPushButton("...", this);
  save_as_button_->setMaximumWidth(30);
  connect(save_as_button_, SIGNAL(clicked()),
          this, SLOT(PromptSaveAsPath()));
  file_spec_layout->addWidget(save_as_button_, 1, 2);

  // Third row: file type
  file_type_label_ = new QLabel("File type: ", this);
  file_spec_layout->addWidget(file_type_label_, 2, 0);
  CreateSpinnerStackedWidget(&file_type_stack_, &file_type_spinner_,
                             &file_type_value_label_);
  file_spec_layout->addWidget(file_type_stack_, 2, 1);

  // Fourth row: file size
  file_size_label_ = new QLabel("File size: ", this);
  file_spec_layout->addWidget(file_size_label_, 3, 0);
  CreateSpinnerStackedWidget(&file_size_stack_, &file_size_spinner_,
                             &file_size_value_label_);
  file_spec_layout->addWidget(file_size_stack_, 3, 1);

  // Fifth row: Mime type
  mime_type_label_ = new QLabel("Mime type: ", this);
  file_spec_layout->addWidget(mime_type_label_, 4, 0);
  CreateSpinnerStackedWidget(&mime_type_stack_, &mime_type_spinner_,
                             &mime_type_value_label_);
  file_spec_layout->addWidget(mime_type_stack_, 4, 1);
}

void DownloadDialog::CreateSpinnerStackedWidget(QStackedWidget** stack,
                                                Spinner** spinner,
                                                QLabel** label) {
  *stack = new QStackedWidget(this);
  *spinner = new Spinner();
  *label = new QLabel();
  (*stack)->addWidget(*spinner);
  (*stack)->addWidget(*label);
  (*stack)->setCurrentIndex(0);
}

void DownloadDialog::SetUpConfigWidgets() {
  config_gbox_ = new QGroupBox("Download Config", this);
  QGridLayout* config_layout = new QGridLayout();
  config_layout->setAlignment(Qt::Alignment(Qt::AlignLeft));
  config_gbox_->setLayout(config_layout);

  num_connections_label_ = new QLabel("Number of connections", this);
  config_layout->addWidget(num_connections_label_, 0, 0);
  num_connections_sbox_ = new QSpinBox(this);
  num_connections_sbox_->setMaximumWidth(100);
  num_connections_sbox_->setRange(1, kMaxConnections);
  int default_num_connections;
  preference_manager_->Get("num_connections", &default_num_connections);
  num_connections_sbox_->setValue(default_num_connections);
  config_layout->addWidget(num_connections_sbox_,  0, 1);
}

void DownloadDialog::StartSpinners() {
  file_type_stack_->setCurrentIndex(0);
  file_size_stack_->setCurrentIndex(0);
  mime_type_stack_->setCurrentIndex(0);

  file_type_spinner_->startAnimation();
  file_size_spinner_->startAnimation();
  mime_type_spinner_->startAnimation();
}

void DownloadDialog::StopSpinners() {
  file_type_spinner_->stopAnimation();
  file_size_spinner_->stopAnimation();
  mime_type_spinner_->stopAnimation();

  file_type_stack_->setCurrentIndex(1);
  file_size_stack_->setCurrentIndex(1);
  mime_type_stack_->setCurrentIndex(1);
}

void DownloadDialog::GetFileSpec(const QString& url) {
  file_type_value_label_->setText("");
  file_size_value_label_->setText("");
  mime_type_value_label_->setText("");
  save_as_edit_->setText("");
  url_edit_->setStyleSheet("");
  url_edit_->setToolTip("");
  StopSpinners();
  QUrl parsed(url);
  if (parsed.scheme().isEmpty() || parsed.host().isEmpty()) {
    return;
  }
  params_.SetFileSize(0);
  params_.SetAccelerable(false);

  FileSpecGetter* getter = new FileSpecGetter(++current_getter_id_, url);
  QThread* thread = new QThread();
  getter->moveToThread(thread);
  connect(thread, SIGNAL(started()), getter, SLOT(Run()));
  connect(getter, SIGNAL(ResultReady(FileSpec)),
          this, SLOT(OnFileSpecReady(FileSpec)));
  connect(getter, SIGNAL(Error(int, QNetworkReply::NetworkError)),
          this, SLOT(OnGetterError(int, QNetworkReply::NetworkError)));
  connect(getter, SIGNAL(Finished()), thread, SLOT(quit()));
  connect(getter, SIGNAL(Finished()), getter, SLOT(deleteLater()));
  connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
  thread->start();
  StartSpinners();
}

void DownloadDialog::OnFileSpecReady(FileSpec spec) {
  if (spec.Id() != current_getter_id_) {
    return;
  }
  QString file_type = "unknown";
  QString file_size = "unknown";
  QString mime_type = "unknown";
  QString save_as = "";

  if (!spec.MimeType().IsNull()) {
    // Remove parameters from mime-type.
    mime_type = spec.MimeType().Get().split(";").first().simplified();
  }
  if (spec.FileSize().Get() > 0) {
    file_size = IntFileSizeToString(spec.FileSize().Get());
  } else {
    num_connections_sbox_->setValue(1);
  }
  num_connections_sbox_->setEnabled(spec.FileSize().Get() > 0);
  params_.SetFileSize(spec.FileSize().Get());
  params_.SetAccelerable(spec.Accelerable());
  QString download_dir;
  preference_manager_->Get("download_dir", &download_dir);

  QString next_download_item = GetNextDownloadItem();
  save_as =
      next_download_item.isEmpty()
          ? SuggestSaveAs(spec.Url(), mime_type, download_dir)
          : next_download_item;
  if (!save_as.isEmpty()) {
    file_type = FileExt(save_as);
  }
  file_type_value_label_->setText(file_type);
  file_size_value_label_->setText(file_size);
  mime_type_value_label_->setText(mime_type);
  save_as_edit_->setText(save_as);
  StopSpinners();
}

void DownloadDialog::OnGetterError(int id, QNetworkReply::NetworkError error) {
  if (id != current_getter_id_) {
    return;
  }
  StopSpinners();
  qDebug() << "Download dialog encountered error " << error;
  url_edit_->setStyleSheet("color: red;");
  url_edit_->setToolTip(QString("Error. %1").arg(error));
  url_edit_->setToolTipDuration(5000);
}

void DownloadDialog::PromptSaveAsPath() {
  QString given_path = save_as_edit_->text();
  QFileInfo info(given_path);
  QString fname = info.fileName();
  // QString prefix = info.baseName();
  QString suffix = info.suffix();
  QString download_dir = info.dir().absolutePath();

  if (!QFile(download_dir).exists()) {
    if (!download_dir.isEmpty()) {
      // TODO(ogaro): Would you like to create it?
      QMessageBox::information(
          this, "Directory does not exist",
          QString("The directory %1 does not exist").arg(download_dir));
    }
    preference_manager_->Get("download_dir", &download_dir);
  }

  QString selected_filter;
  QString filter = QString("%1%2").arg(suffix).arg(
      suffix.isEmpty() ? "" : ";;*");
  QString download_path = QFileDialog::getSaveFileName(
      this,
      "Save as",
      JoinPath(download_dir, fname),
      filter,
      &selected_filter);

  // Add a file extension if it is missing.
  // TODO(ogaro): Is this necessary?
  QString new_suffix = FileExt(download_path);
  if (new_suffix.isEmpty() && selected_filter != "*") {
    if (selected_filter.startsWith(".")) {
      download_path += selected_filter;
    } else {
      download_path += "." + selected_filter;
    }
  }

  if (!download_path.isEmpty()) {
    save_as_edit_->setText(download_path);
    download_button_->setFocus();
  }
}

QString DownloadDialog::GetNextDownloadItem() {
  // TODO(ogaro): Implement.
  return QString();
}

void DownloadDialog::CopyUrlFromClipboard() {
  QUrl url(clipboard_->text());
  if (!url.scheme().isEmpty() && !url.host().isEmpty()) {
    url_edit_->setText(url.toString());
    url_edit_->setCursorPosition(0);
  }
}

void DownloadDialog::LaunchDownload() {
  params_.SetUrl(url_edit_->text());
  params_.SetSaveAs(save_as_edit_->text());
  params_.SetNumConnections(num_connections_sbox_->value());
  accept();
}

void DownloadDialog::reject() {
  params_.SetUrl("");
  QDialog::reject();
}
