#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QTreeWidgetItem>
#include <QStackedWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QToolButton>
#include <QColor>
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <unordered_map>
#include <QGridLayout>
#include "qaccelerator-db.h"
#include "speed-grapher.h"

class PreferencePage : public QWidget {
    Q_OBJECT
 public:
  PreferencePage(QWidget* parent, PreferenceManager* preference_manager);

 protected slots:
  virtual void ResetDefaults() = 0;

 protected:
  virtual void SetFieldValuesFromDb() = 0;

  PreferenceManager* preference_manager_;
  void CreateResetButton();
};

class GeneralPage : public PreferencePage {
    Q_OBJECT
 public:
  GeneralPage(QWidget* parent, PreferenceManager* preference_manager);

 protected slots:
  virtual void ResetDefaults() override;

 private slots:
  void OnDownloadDirChanged(const QString& text);
  void PromptDownloadDir();
  void UpdateNumConnections(int newValue);
  void UpdateConcurrentCap(int newValue);

 protected:
  virtual void SetFieldValuesFromDb() override;

 private:
  void ConnectSlots();
  void DisconnectSlots();

  QPushButton* download_dir_button_;
  QSpinBox* num_connections_spin_;
  QSpinBox* concurrent_cap_spin_;
  QLineEdit* download_dir_edit_;
};

class PreferencesDialog : public QDialog {
    Q_OBJECT
 public:
  PreferencesDialog(QWidget* parent, PreferenceManager* preference_manager);

 private slots:
  void OnTreeItemChanged(QTreeWidgetItem* prev_item,
                         QTreeWidgetItem* curr_item);

 private:
  QStackedWidget* stack_;
};

class ColorButton : public QToolButton {
    Q_OBJECT
 public:
  ColorButton(QWidget* parent, const QColor& color);
  void SetColor(const QColor& color);
  const QColor& Color() { return color_; }

 private:
  QColor color_;
  QLabel* colored_label_;
  QGraphicsOpacityEffect* effect_;
};

class ChartPage : public PreferencePage {
    Q_OBJECT
 public:
  ChartPage(QWidget* parent, PreferenceManager* preference_manager,
            SpeedGrapherState state);

 protected slots:
  virtual void ResetDefaults() override;
  void OnIndicatorDotSizeChanged(int newSize);

 private slots:
  void OnColorButtonClicked(const QString& preference_root);

 protected:
  void CreateControls();
  void NewColorRow(const QString& preference_root, const QString& label_text);
  virtual void SetFieldValuesFromDb() override;
  void ConnectSlots();
  void DisconnectSlots();

  SpeedGrapher* grapher_;
  std::unordered_map<QString, ColorButton*> color_buttons_;
  QString state_;
  QGridLayout* controls_layout_;
  QSpinBox* indicator_dot_size_spin_;
};

class InProgressChartPage : public ChartPage {
    Q_OBJECT
 public:
  InProgressChartPage(QWidget* parent, PreferenceManager* preference_manager);
};

class PausedChartPage : public ChartPage {
    Q_OBJECT
 public:
  PausedChartPage(QWidget* parent, PreferenceManager* preference_manager);
};

class CompletedChartPage : public ChartPage {
    Q_OBJECT
 public:
  CompletedChartPage(QWidget* parent, PreferenceManager* preference_manager);
};

#endif // PREFERENCES_DIALOG_H

