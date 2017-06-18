// TODO(ogaro): Use const ref in signal/slots.
#ifndef DOWNLOAD_DIALOG_H_
#define DOWNLOAD_DIALOG_H_

#include "qaccelerator-utils.h"
#include "qaccelerator-db.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <memory>
#include <QDialog>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include "spinner.h"
#include <QStackedWidget>
#include <QSpinBox>
#include <QFileDialog>
#include <QFrame>
#include <QThread>
#include <QMetaType>

typedef QProgressIndicator Spinner;

class DownloadDialog : public QDialog {
    Q_OBJECT
 public:
  /*
  static DownloadParams GetParams(PreferenceManager* preference_manager) {
    DownloadDialog dialog(preference_manager);
    QDialog::DialogCode result = static_cast<QDialog::DialogCode>(
          dialog.exec());
    DownloadParams params = dialog.Params();
    if (result == QDialog::DialogCode::Rejected) {
      params.SetUrl("");
    }
    // DownloadParams params = dialog.Params();
    return dialog.Params();
  }*/

  DownloadDialog(QWidget* parent, PreferenceManager* preference_manager);
  ~DownloadDialog();
  void CopyUrlFromClipboard();
  const DownloadParams& Params() { return params_; }
  void ResetFields();

 public slots:
  void OnFileSpecReady(FileSpec spec);
  void OnGetterError(int id, QNetworkReply::NetworkError error);
  virtual void reject() override;

 private slots:
  void GetFileSpec(const QString& url);
  void PromptSaveAsPath();
  void LaunchDownload();

 private:
  void SetUpFileSpecWidgets();
  void SetUpConfigWidgets();
  void CreateSpinnerStackedWidget(QStackedWidget** stack,
                                  Spinner** spinner, QLabel** label);
  void StartSpinners();
  void StopSpinners();
  QString GetNextDownloadItem();


  PreferenceManager* preference_manager_;
  QGroupBox* file_spec_gbox_;

  QLabel* url_label_;
  QLineEdit* url_edit_;

  QLabel* save_as_label_;
  QLineEdit* save_as_edit_;
  QPushButton* save_as_button_;

  QLabel* file_type_label_;
  QStackedWidget* file_type_stack_;
  Spinner* file_type_spinner_;
  QLabel* file_type_value_label_;

  QLabel* file_size_label_;
  QStackedWidget* file_size_stack_;
  Spinner* file_size_spinner_;
  QLabel* file_size_value_label_;

  QLabel* mime_type_label_;
  QStackedWidget* mime_type_stack_;
  Spinner* mime_type_spinner_;
  QLabel* mime_type_value_label_;

  QPushButton* download_button_;
  QGroupBox* config_gbox_;
  QLabel* num_connections_label_;
  QSpinBox* num_connections_sbox_;

  DownloadParams params_;
  QFrame* divider_;
  QClipboard* clipboard_;

  int current_getter_id_;
};

#endif  // DOWNLOAD_DIALOG_H_
