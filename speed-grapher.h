#ifndef SPEED_GRAPHER_H_
#define SPEED_GRAPHER_H_

#define _USE_MATH_DEFINES 1;
#include <math.h>
#include <QGraphicsView>
#include <QSize>
#include <vector>
#include <string>
#include <QVariant>
#include <QPainterPath>
#include <tuple>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <QString>
#include <QObject>
#include <QTimer>


class SpeedGrapher : public QGraphicsView {
 public:
  SpeedGrapher(QWidget* parent,
               const QSize& size,
               int num_vertical_gridlines,
               int num_horizontal_gridlines);
  SpeedGrapher(QWidget* parent, const QSize& size)
    : SpeedGrapher(parent, size, 15, 10) {}
  SpeedGrapher(QWidget* parent) : SpeedGrapher(parent, QSize(650, 100)) {}

  // Refreshes the plot. This is typically called right after AddDataPoint().
  void UpdatePlot(bool show_indicators);

  // Getters and setters.
  const std::vector<double>& GetData() { return ys_; }
  void SetData(const std::vector<double>& ys) {
    if (ys.empty()) {
      return;
    }
    ys_.assign(ys.begin(), ys.end());
  }
  void SetProgress(const double progress);
  void SetStyleAttribute(const QString& style_attr,
                         const QVariant& style_value);
  const QVariant& GetStyleAttribute(const QString& style_attr);
  double GetDoubleStyleAttribute(const QString& style_attr) {
    return GetStyleAttribute(style_attr).toDouble();
  }
  QString GetStringStyleAttributeS(const QString& style_attr) {
    return GetStyleAttribute(style_attr).toString();
  }

  // Adds a new data point to the speed grapher
  void AddDataPoint(double y, double progress,
                    const QString& indicator_text);
  void AddDataPoint(double y, double progress) {
    AddDataPoint(y, progress, "");
  }

 private:
  void DrawIndicators();
  QPainterPath* ComputeSeriesPath(double* last_x, double* last_y);
  std::unique_ptr<QPen> MkPen(const QString& pen_name);
  std::unique_ptr<QBrush> MkBrush(const QString& brush_name);
  void DrawGridlines();

  QSize size_;
  std::vector<double> ys_;
  double progress_;
  QMap<QString, QVariant> style_dict_;
  QGraphicsScene scene_;

  // TODO(ogaro): These should probably eventually be added as style
  // attributes.
  double yprop_;
  int num_vertical_gridlines_;
  int num_horizontal_gridlines_;
  double indicator_text_margin_bottom_;
  double indicator_text_margin_right_;
  QString indicator_text_;
};

class SpeedGrapherDemo : public QWidget {
  Q_OBJECT

 public:
  SpeedGrapherDemo(QTimer* timer)
    : num_points_(1000),
      num_oscillations_(4.8),
      interval_(100),
      iter_(0) {
  speed_grapher_ = new SpeedGrapher(this);
  speed_grapher_->show();
  // Compute data points for the function abs(sin(Kx) where K is a constant.
  for (int i = 0; i < num_points_; ++i) {
    ys_.push_back(fabs(sin(
        i * 2 * M_PI * num_oscillations_ / (num_points_ - 1))));
  }
  connect(timer, SIGNAL(timeout()), this, SLOT(Run()));
  timer->setInterval(interval_);
  }
  virtual ~SpeedGrapherDemo() {}

public slots:
 void Run() {
   if (iter_ < num_points_) {
     double progress = (iter_ + 1.0) / num_points_;
     speed_grapher_->AddDataPoint(ys_[iter_], progress);
     iter_ += 1;
   } else {
     speed_grapher_->UpdatePlot(false);
   }
 }

private:
 SpeedGrapher* speed_grapher_;
 int num_points_;
 double num_oscillations_;
 int interval_;
 std::vector<double> ys_;
 int iter_;
};

#endif // SPEED_GRAPHER_H_
