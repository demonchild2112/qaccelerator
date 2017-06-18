#include "preferences-dialog.h"
#include "qaccelerator-utils.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeWidget>
#include <QSplitter>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QColorDialog>
#include <vector>

using std::vector;

namespace {
QString PreferenceRootToColor(const QString& state,
                              const QString& preference_root) {
  return QString("%1_%2_color").arg(state).arg(preference_root);
}

QString PreferenceRootToAlpha(const QString& state,
                              const QString& preference_root) {
  return QString("%1_%2_alpha").arg(state).arg(preference_root);
}
}

PreferencePage::PreferencePage(QWidget* parent,
                               PreferenceManager* preference_manager)
  : QWidget(parent) {
  preference_manager_ = preference_manager;
  QVBoxLayout* main_layout = new QVBoxLayout();
  setLayout(main_layout);
  main_layout->setAlignment(Qt::Alignment(Qt::AlignTop | Qt::AlignLeft));
  main_layout->setContentsMargins(0, 0, 0, 0);
}

void PreferencePage::CreateResetButton() {
  QPushButton* reset_button = new QPushButton("Reset to defaults", this);
  QVBoxLayout* main_layout = dynamic_cast<QVBoxLayout*>(layout());
  main_layout->addWidget(reset_button, 0, Qt::AlignBottom | Qt::AlignRight);
  connect(reset_button, SIGNAL(clicked()),
          this, SLOT(ResetDefaults()));
}

GeneralPage::GeneralPage(QWidget* parent,
                         PreferenceManager* preference_manager)
    : PreferencePage(parent, preference_manager) {
  QGroupBox* controls_gbox = new QGroupBox(this);
  QGridLayout* controls_layout = new QGridLayout(controls_gbox);
  controls_gbox->setStyleSheet(kUmemeStyle);
  controls_layout->setAlignment(Qt::Alignment(Qt::AlignTop | Qt::AlignLeft));
  QGroupBox* download_dir_gbox = new QGroupBox(this);
  download_dir_gbox->setStyleSheet(kUmemeStyle);
  layout()->addWidget(download_dir_gbox);
  QGridLayout* download_dir_layout = new QGridLayout(download_dir_gbox);
  controls_gbox->setMaximumHeight(200);
  layout()->addWidget(controls_gbox);

  // First row: Default download dir.
  QLabel* download_dir_label = new QLabel("Download files to", this);
  download_dir_layout->addWidget(download_dir_label, 0, 0);
  download_dir_edit_ = new QLineEdit(this);
  // download_dir_edit_->setMaximumWidth(400);
  download_dir_layout->addWidget(download_dir_edit_, 0, 1);
  download_dir_button_ = new QPushButton("...", this);
  download_dir_button_->setMaximumWidth(30);
  download_dir_layout->addWidget(download_dir_button_, 0, 2);

  // Second row: Default number of connections
  QLabel* num_connections_label = new QLabel("Default number of connections",
                                             this);
  controls_layout->addWidget(num_connections_label, 0, 0);
  num_connections_spin_ = new QSpinBox(this);
  num_connections_spin_->setRange(1, kMaxConnections);
  num_connections_spin_->setMaximumWidth(100);
  controls_layout->addWidget(num_connections_spin_, 0, 1);

  // Third row: Maximum number of concurrent active downloads
  QLabel* concurrent_cap_label = new QLabel(
      "Maximum number of concurrent active downloads", this);
  controls_layout->addWidget(concurrent_cap_label, 1, 0);
  concurrent_cap_spin_ = new QSpinBox(this);
  concurrent_cap_spin_->setMinimum(1);
  concurrent_cap_spin_->setMaximumWidth(100);
  controls_layout->addWidget(concurrent_cap_spin_, 1, 1);
  CreateResetButton();
  SetFieldValuesFromDb();
  ConnectSlots();
}

void GeneralPage::OnDownloadDirChanged(const QString& new_dir_path) {
  // QString new_dir_path = download_dir_edit_->text();
  QFileInfo info(new_dir_path);
  if (info.exists() && info.isDir()) {
    download_dir_edit_->setStyleSheet("");
    preference_manager_->Set("download_dir", new_dir_path);
  } else {
    download_dir_edit_->setStyleSheet("color: red");
  }
}

void GeneralPage::PromptDownloadDir() {
  QString curr_dir_path;
  preference_manager_->Get("download_dir", &curr_dir_path);
  QString new_dir_path = QFileDialog::getExistingDirectory(this,
      "Choose download directory", curr_dir_path);
  if (!new_dir_path.isEmpty()) {
    download_dir_edit_->setText(new_dir_path);
  }
}

void GeneralPage::SetFieldValuesFromDb() {
  QString download_dir;
  int num_connections;
  int concurrent_cap;
  preference_manager_->Get("download_dir", &download_dir);
  preference_manager_->Get("num_connections", &num_connections);
  preference_manager_->Get("concurrent_cap", &concurrent_cap);
  download_dir_edit_->setText(download_dir);
  num_connections_spin_->setValue(num_connections);
  concurrent_cap_spin_->setValue(concurrent_cap);
}

void GeneralPage::ResetDefaults() {
  DisconnectSlots();
  QString download_dir;
  int num_connections;
  int concurrent_cap;
  preference_manager_->GetDefault("download_dir", &download_dir);
  preference_manager_->GetDefault("num_connections", &num_connections);
  preference_manager_->GetDefault("concurrent_cap", &concurrent_cap);
  preference_manager_->Set("download_dir", download_dir);
  preference_manager_->Set("num_connections", num_connections);
  preference_manager_->Set("concurrent_cap", concurrent_cap);
  SetFieldValuesFromDb();
  ConnectSlots();
}

void GeneralPage::ConnectSlots() {
  connect(download_dir_button_, SIGNAL(clicked()),
          this, SLOT(PromptDownloadDir()));
  connect(num_connections_spin_, SIGNAL(valueChanged(int)),
          this, SLOT(UpdateNumConnections(int)));
  connect(concurrent_cap_spin_, SIGNAL(valueChanged(int)),
          this, SLOT(UpdateConcurrentCap(int)));
  connect(download_dir_edit_, SIGNAL(textChanged(QString)),
          this, SLOT(OnDownloadDirChanged(QString)));
}

void GeneralPage::UpdateNumConnections(int newValue) {
  preference_manager_->Set("num_connections", newValue);
}

void GeneralPage::UpdateConcurrentCap(int newValue) {
  preference_manager_->Set("concurrent_cap", newValue);
}

void GeneralPage::DisconnectSlots() {
  disconnect(download_dir_button_, SIGNAL(clicked()), 0, 0);
  disconnect(num_connections_spin_, SIGNAL(valueChanged(int)), 0, 0);
  disconnect(concurrent_cap_spin_, SIGNAL(valueChanged(int)), 0, 0);
  disconnect(download_dir_edit_, SIGNAL(textChanged(QString)), 0, 0);
}

ColorButton::ColorButton(QWidget *parent, const QColor &color)
    : QToolButton(parent) {
  setMinimumSize(QSize(30, 30));
  setMaximumSize(QSize(30, 30));
  setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
  QHBoxLayout* main_layout = new QHBoxLayout();
  setLayout(main_layout);
  main_layout->setContentsMargins(0, 0, 0, 0);
  colored_label_ = new QLabel(this);
  colored_label_->setMinimumSize(QSize(23, 23));
  colored_label_->setMaximumSize(QSize(23, 23));
  main_layout->addWidget(colored_label_);
  effect_ = new QGraphicsOpacityEffect(colored_label_);
  colored_label_->setGraphicsEffect(effect_);
  setStyleSheet(
      "QToolButton {"
      "  background-color: #fff;"
      "  border: 1px outset #999;"
      "}"
      "QToolButton:hover:!pressed {"
      "  border: 1px inset #666;"
      "}"
      "QLabel{"
      "  border: 1px solid #ccc;"
      "  margin-left: 1px;"
      "  margin-top: 1px"
      "}");
  SetColor(color);
}

void ColorButton::SetColor(const QColor& color) {
  color_ = QColor(color);
  colored_label_->setStyleSheet(
        QString("background-color: %1;").arg(color_.name()));
  effect_->setOpacity(color_.alphaF());
}

ChartPage::ChartPage(QWidget *parent, PreferenceManager *preference_manager,
                     SpeedGrapherState state)
    : PreferencePage(parent, preference_manager) {
  grapher_ = new SpeedGrapher(this, QSize(600, 100));
  indicator_dot_size_spin_ = nullptr;
  QGroupBox* controls_gbox = new QGroupBox(this);
  state_ = ToString(state);
  controls_layout_ = new QGridLayout(controls_gbox);
  controls_layout_->setAlignment(Qt::Alignment(Qt::AlignTop | Qt::AlignLeft));
  controls_layout_->setContentsMargins(0, 0, 0, 0);
  vector<double> ys = {0, 1384448, 1073152, 1462272, 1662976, 1806336,
                       1798144, 1912832, 1384448, 0, 143360, 593920,
                       557056, 1155072, 925696, 995328, 1507328, 1404928};
  grapher_->SetData(ys);
  double progress = (state_ == "completed" ? 1.0 : 0.4);
  grapher_->AddDataPoint(ys.back(), progress, "4.20 MB/s");
  layout()->addWidget(grapher_);
  layout()->addWidget(controls_gbox);
  CreateControls();
  SetFieldValuesFromDb();
}

void ChartPage::OnIndicatorDotSizeChanged(int new_size) {
  preference_manager_->Set(state_ + "_indicator_dot_size", new_size);
  grapher_->SetStyleAttribute("indicator_dot_size", new_size);
  grapher_->UpdatePlot(state_ == "in_progress");
}

void ChartPage::OnColorButtonClicked(const QString &preference_root) {
  ColorButton* button = color_buttons_[preference_root];
  QColor new_color = QColorDialog::getColor(button->Color(), this,
                                           "Choose color",
                                           QColorDialog::ShowAlphaChannel);
  if (!new_color.isValid()) {
    return;
  }
  button->SetColor(new_color);
  QString color_preference = PreferenceRootToColor(state_, preference_root);
  QString alpha_preference = PreferenceRootToAlpha(state_, preference_root);
  preference_manager_->Set(color_preference, new_color.name());
  preference_manager_->Set(alpha_preference, new_color.alphaF());
  grapher_->SetStyleAttribute(color_preference.replace(state_ + "_", ""),
                             new_color.name());
  grapher_->SetStyleAttribute(alpha_preference.replace(state_ + "_", ""),
                             new_color.alphaF());
  grapher_->UpdatePlot(state_ == "in_progress");
}

void ChartPage::NewColorRow(const QString &preference_root,
                       const QString &label_text) {
  int index = controls_layout_->rowCount();
  controls_layout_->addWidget(new QLabel(label_text, this), index, 0);
  ColorButton* preference_button = new ColorButton(this, QColor());
  color_buttons_[preference_root] = preference_button;
  controls_layout_->addWidget(preference_button, index, 1);
}

void ChartPage::SetFieldValuesFromDb() {
  for (const auto& entry : color_buttons_) {
    QString color_name;
    preference_manager_->Get(PreferenceRootToColor(state_, entry.first),
                             &color_name);
    double alpha;
    preference_manager_->Get(PreferenceRootToAlpha(state_, entry.first),
                             &alpha);
    QColor color;
    color.setNamedColor(color_name);
    color.setAlphaF(alpha);
    entry.second->SetColor(color);
  }
  if (indicator_dot_size_spin_ != nullptr) {
    int indicator_dot_size;
    preference_manager_->Get(state_ + "_indicator_dot_size",
                             &indicator_dot_size);
    indicator_dot_size_spin_->setValue(indicator_dot_size);
  }
}

void ChartPage::ResetDefaults() {
  for (const auto& entry : color_buttons_) {
    preference_manager_->SetDefault(PreferenceRootToColor(state_, entry.first));
    preference_manager_->SetDefault(PreferenceRootToAlpha(state_, entry.first));
  }
  SpeedGrapherState state = FromString(state_);
  if (state == SpeedGrapherState::IN_PROGRESS) {
    preference_manager_->SetDefault(state_ + "_indicator_dot_size");
  }
  DisconnectSlots();
  SetFieldValuesFromDb();
  preference_manager_->ApplySavedPreferences(
      state,
      grapher_,
      state == SpeedGrapherState::IN_PROGRESS);
  ConnectSlots();
}

void ChartPage::ConnectSlots() {
  for (const auto& entry : color_buttons_) {
    const QString& preference_root = entry.first;
    ColorButton* button = entry.second;
    connect(button, &ColorButton::clicked, [=] {
      OnColorButtonClicked(preference_root);
    });
  }
}

void ChartPage::DisconnectSlots() {
  for (const auto& entry : color_buttons_) {
    ColorButton* button = entry.second;
    disconnect(button, SIGNAL(clicked()), 0, 0);
  }
}

InProgressChartPage::InProgressChartPage(QWidget *parent,
                                         PreferenceManager *preference_manager)
    : ChartPage(parent, preference_manager, SpeedGrapherState::IN_PROGRESS) {
  CreateResetButton();
  dynamic_cast<QVBoxLayout*>(layout())->addSpacing(10);
}

PausedChartPage::PausedChartPage(QWidget *parent,
                                 PreferenceManager *preference_manager)
    : ChartPage(parent, preference_manager, SpeedGrapherState::PAUSED) {
  CreateResetButton();
  dynamic_cast<QVBoxLayout*>(layout())->addSpacing(170);
  grapher_->UpdatePlot(false);
}

CompletedChartPage::CompletedChartPage(QWidget *parent,
                                 PreferenceManager *preference_manager)
    : ChartPage(parent, preference_manager, SpeedGrapherState::COMPLETED) {
  CreateResetButton();
  dynamic_cast<QVBoxLayout*>(layout())->addSpacing(170);
}

void ChartPage::CreateControls() {
  NewColorRow(
      "chart_brush",
      "Color that fills the space up to the progress level");
  NewColorRow(
      "curve_brush",
      "Color that fills the space under the curve line");
  NewColorRow(
      "background_brush",
      "Background color of the progress bar");
  NewColorRow("curve_pen", "Color of the curve line");
  if (state_ == "in_progress") {
    NewColorRow(
        "indicator_line_pen",
        "Color of the horizontal indicator line");
    NewColorRow(
        "indicator_text",
        "Color of indicator line's caption");
    NewColorRow(
        "indicator_dot_brush",
        "Color of the indicator dot");
    NewColorRow(
        "indicator_dot_pen",
        "Color of the border around the indicator dot");
    QLabel* indicator_dot_size_label = new QLabel("Size of the indicator dot",
                                                  this);
    int index = controls_layout_->rowCount();
    controls_layout_->addWidget(indicator_dot_size_label, index, 0);
    indicator_dot_size_spin_ = new QSpinBox(this);
    connect(indicator_dot_size_spin_, SIGNAL(valueChanged(int)),
            this, SLOT(OnIndicatorDotSizeChanged(int)));
    controls_layout_->addWidget(indicator_dot_size_spin_, index, 1);
  }
  SpeedGrapherState state = FromString(state_);
  preference_manager_->ApplySavedPreferences(
      state,
      grapher_,
      state == SpeedGrapherState::IN_PROGRESS);
  ConnectSlots();
}

PreferencesDialog::PreferencesDialog(QWidget* parent,
                                     PreferenceManager *preference_manager)
    : QDialog(parent) {
  // setGeometry(250, 250, 760, 600);
  QVBoxLayout* main_layout = new QVBoxLayout();
  main_layout->setAlignment(Qt::Alignment(Qt::AlignTop | Qt::AlignLeft));
  setLayout(main_layout);

  GeneralPage* general_page = new GeneralPage(this, preference_manager);
  InProgressChartPage* in_progress_chart_page = new InProgressChartPage(
     this, preference_manager);
  PausedChartPage* paused_chart_page = new PausedChartPage(this,
                                                           preference_manager);
  CompletedChartPage* completed_chart_page = new CompletedChartPage(
      this, preference_manager);
  QTreeWidget* tree = new QTreeWidget(this);
  tree->setColumnCount(1);
  connect(tree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
          this, SLOT(OnTreeItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
  tree->setHeaderLabel("Preferences");
  QTreeWidgetItem* general_tree_item = new QTreeWidgetItem(tree);
  general_tree_item->setText(0, "General");
  QTreeWidgetItem* chart_tree_item = new QTreeWidgetItem(tree);
  chart_tree_item->setText(0, "Progress Chart");
  (new QTreeWidgetItem(chart_tree_item))->setText(0, "Active");
  (new QTreeWidgetItem(chart_tree_item))->setText(0, "Paused");
  (new QTreeWidgetItem(chart_tree_item))->setText(0, "Completed");
  chart_tree_item->setExpanded(true);
  stack_ = new QStackedWidget(this);
  stack_->addWidget(general_page);
  stack_->addWidget(in_progress_chart_page);
  stack_->addWidget(paused_chart_page);
  stack_->addWidget(completed_chart_page);
  stack_->setCurrentIndex(0);
  QSplitter* splitter = new QSplitter(this);
  tree->resizeColumnToContents(0); // Doen't work. Find out why.
  splitter->addWidget(tree);
  splitter->addWidget(stack_);
  //splitter->setStretchFactor(0, 3);
  //splitter->setStretchFactor(1, 4);
  main_layout->addWidget(splitter);
  /*
  QFrame* hrule = new QFrame(this);
  hrule->setFrameShape(QFrame::HLine);
  hrule->setFrameShadow(QFrame::Sunken);
  main_layout->addWidget(hrule);
  main_layout->addSpacing(20);
  QPushButton* close_button = new QPushButton("Close", this);
  connect(close_button, SIGNAL(clicked()),
          this, SLOT(close()));
  close_button->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,
                                          QSizePolicy::Fixed));
  main_layout->addWidget(close_button, 0, Qt::AlignRight);*/
  setFixedSize(sizeHint());
  setWindowTitle("QAccelerator");
  setWindowIcon(QIcon(":/images/qx_flash.png"));
}

void PreferencesDialog::OnTreeItemChanged(QTreeWidgetItem *curr_item,
                                     QTreeWidgetItem *prev_item) {
  QString category = curr_item->text(0);
  if (category == "General") {
    stack_->setCurrentIndex(0);
  } else if (category == "Active") {
    stack_->setCurrentIndex(1);
  } else if (category == "Paused") {
    stack_->setCurrentIndex(2);
  } else if (category == "Completed") {
    stack_->setCurrentIndex(3);
  }
}
