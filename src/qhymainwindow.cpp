/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2015  <copyright holder> <email>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "qhymainwindow.h"
#include "qhydriver.h"
#include "qhyccdimager.h"
#include "ui_qhymainwindow.h"
#include <functional>
#include "utils.h"
#include "statusbarinfowidget.h"
#include "saveimages.h"
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSettings>
#include <QThread>

using namespace std;
using namespace std::placeholders;

class DisplayImage : public QObject, public ImageHandler {
  Q_OBJECT
public:
  DisplayImage(QObject* parent = 0);
  virtual void handle(const ImageDataPtr& imageData);
  fps capture_fps;
signals:
  void gotImage(const QImage &);
  void captureFps(double fps);
};

DisplayImage::DisplayImage(QObject* parent): QObject(parent), capture_fps([=](double fps){ emit captureFps(fps);})
{

}

void DisplayImage::handle(const ImageDataPtr& imageData)
{
  capture_fps.add_frame();
  auto ptrCopy = new ImageDataPtr(imageData);
  QImage image(imageData->data(), imageData->width(), imageData->height(), QImage::Format_Grayscale8, [](void *data){ delete reinterpret_cast<ImageDataPtr*>(data); }, ptrCopy);
  emit gotImage(image);
}



class QHYMainWindow::Private {
public:
  Private(QHYMainWindow *q);
  shared_ptr<Ui::QHYMainWindow> ui;
  QHYDriver driver;
  QHYCCDImagerPtr imager;
  void rescan_devices();
  QSettings settings;
  void saveState();
  QBoxLayout *settings_layout;
  QGraphicsScene *scene;
  double zoom;
  StatusBarInfoWidget *statusbar_info_widget;
  shared_ptr<DisplayImage> displayImage = make_shared<DisplayImage>();
  shared_ptr<SaveImages> saveImages = make_shared<SaveImages>();
  QRect imageRect;
  
  void connectCamera(const QHYDriver::Camera &camera);
  void cameraDisconnected();
  void enableUIWidgets(bool cameraConnected);
private:
  QHYMainWindow *q;
};


class CameraSettingWidget : public QWidget {
  Q_OBJECT
public:
  CameraSettingWidget(const QHYCCDImager::Setting &setting, const QHYCCDImagerPtr &imagerPtr, QWidget* parent = 0);  
};

CameraSettingWidget::CameraSettingWidget(const QHYCCDImager::Setting& setting, const QHYCCDImagerPtr& imagerPtr, QWidget* parent): QWidget(parent)
{
  setObjectName("setting_%1"_q % setting.name);
  auto layout = new QHBoxLayout;
  setLayout(layout);
  layout->addWidget(new QLabel(tr(qPrintable(setting.name))));
  QDoubleSpinBox *spinbox = new QDoubleSpinBox;
  spinbox->setMinimum(setting.min);
  spinbox->setMaximum(setting.max);
  spinbox->setSingleStep(setting.step != 0 ? setting.step : 0.1);
  spinbox->setValue(setting.value);
  layout->addWidget(spinbox);
  QHYCCDImager *imager = imagerPtr.get();
  connect(spinbox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [=] (double v) {
    auto s = setting;
    s.value = v;
        imager->setSetting(s);
  });
}


QHYMainWindow::Private::Private(QHYMainWindow* q) : ui{make_shared<Ui::QHYMainWindow>()}, settings{"GuLinux", "QHYImager"}, q{q}
{
}


QHYMainWindow::~QHYMainWindow()
{
  if(d->imager)
    d->imager->stopLive();
}

void QHYMainWindow::Private::saveState()
{
  settings.setValue("dock_settings", q->saveState());
}


QHYMainWindow::QHYMainWindow(QWidget* parent, Qt::WindowFlags flags) : dpointer(this)
{
    d->ui->setupUi(this);
    d->scene = new QGraphicsScene(this);
    restoreState(d->settings.value("dock_settings").toByteArray());
    connect(d->ui->action_devices_rescan, &QAction::triggered, bind(&Private::rescan_devices, d.get()));
    
    d->ui->image->setScene(d->scene);
    auto dockWidgetToggleVisibility = [=](QDockWidget *widget, bool visible){ widget->setVisible(visible); };
    auto dockWidgetVisibleCheck = [=](QAction *action, QDockWidget *widget) { action->setChecked(widget->isVisible()); };
    auto setupDockWidget = [=](QAction *action, QDockWidget *widget){
      dockWidgetVisibleCheck(action, widget);
      connect(action, &QAction::triggered, bind(dockWidgetToggleVisibility, widget, _1));
      connect(widget, &QDockWidget::visibilityChanged, bind(dockWidgetVisibleCheck, action, widget));
      connect(widget, &QDockWidget::dockLocationChanged, bind(&Private::saveState, d.get()));
      connect(widget, &QDockWidget::topLevelChanged, bind(&Private::saveState, d.get()));
      connect(widget, &QDockWidget::visibilityChanged, bind(&Private::saveState, d.get()));
    };
    d->zoom = 1;
    auto zoom = [=](qreal scale) { d->ui->image->scale(scale, scale); };
    connect(d->ui->actionZoom_In, &QAction::triggered, [=]{ zoom(1.05); });
    connect(d->ui->actionZoom_Out, &QAction::triggered, [=]{ zoom(0.95); });
    connect(d->ui->actionFit_to_window, &QAction::triggered, [=]{ d->ui->image->fitInView(d->imageRect, Qt::KeepAspectRatio); });
    connect(d->ui->actionActual_Size, &QAction::triggered, [=]{ d->ui->image->setTransform({}); });
    
    
    connect(d->ui->start_recording, &QPushButton::clicked, [=]{
      d->saveImages->startRecording();
    });
    connect(d->ui->stop_recording, &QPushButton::clicked, [=]{
      d->saveImages->endRecording();
    });
    setupDockWidget(d->ui->actionChip_Info, d->ui->chipInfoWidget);
    setupDockWidget(d->ui->actionCamera_Settings, d->ui->camera_settings);
    setupDockWidget(d->ui->actionRecording, d->ui->recording);
    
    d->ui->settings_frame->setLayout(d->settings_layout = new QVBoxLayout);
    d->ui->statusbar->addPermanentWidget(d->statusbar_info_widget = new StatusBarInfoWidget(), 1);
    d->rescan_devices();
    connect(d->displayImage.get(), &DisplayImage::gotImage, this, [=](const QImage &image) {
      d->scene->clear();
      d->scene->addPixmap(QPixmap::fromImage(image));
      d->imageRect = image.rect();
    }, Qt::QueuedConnection);
    // TODO: GUI for this
    d->saveImages->setOutput("/tmp/out.ser");
    connect(d->displayImage.get(), &DisplayImage::captureFps, d->statusbar_info_widget, &StatusBarInfoWidget::captureFPS, Qt::QueuedConnection);
    connect(d->saveImages.get(), &SaveImages::saveFPS, d->statusbar_info_widget, &StatusBarInfoWidget::saveFPS, Qt::QueuedConnection);
    connect(d->saveImages.get(), &SaveImages::savedFrames, d->statusbar_info_widget, &StatusBarInfoWidget::savedFrames, Qt::QueuedConnection);
    connect(d->ui->actionDisconnect, &QAction::triggered, [=]{ d->imager->stopLive(); d->imager.reset(); d->cameraDisconnected(); });
}


void QHYMainWindow::Private::rescan_devices()
{
  ui->menu_device_load->clear();
  for(auto device: driver.cameras()) {
    auto action = ui->menu_device_load->addAction(device.name());
    QObject::connect(action, &QAction::triggered, bind(&Private::connectCamera, this, device));
  }
}

void QHYMainWindow::Private::connectCamera(const QHYDriver::Camera& camera)
{
  imager = make_shared<QHYCCDImager>(camera, QList<ImageHandlerPtr>{displayImage, saveImages});
  if(!imager)
    return;
  imager->startLive();
  statusbar_info_widget->deviceConnected(imager->name());
  ui->camera_name->setText(imager->name());
  ui->camera_chip_size->setText(QString("%1x%2").arg(imager->chip().width, 2).arg(imager->chip().height, 2));
  ui->camera_bpp->setText("%1"_q % imager->chip().bpp);
  ui->camera_pixels_size->setText(QString("%1x%2").arg(imager->chip().pixelwidth, 2).arg(imager->chip().pixelheight, 2));
  ui->camera_resolution->setText(QString("%1x%2").arg(imager->chip().xres, 2).arg(imager->chip().yres, 2));

  auto settings_widgets = ui->settings_frame->findChildren<QWidget*>(QRegularExpression{"setting_.*"});
  for_each(begin(settings_widgets), end(settings_widgets), bind(&QWidget::deleteLater, _1));
  for(auto setting: imager->settings()) {
    qDebug() << "adding setting: " << setting;
    settings_layout->addWidget(new CameraSettingWidget{setting, imager});
  }
  enableUIWidgets(true);
}


void QHYMainWindow::Private::cameraDisconnected()
{
  enableUIWidgets(false);
  ui->camera_name->clear();
  ui->camera_chip_size->clear();
  ui->camera_bpp->clear();
  ui->camera_pixels_size->clear();
  ui->camera_resolution->clear();
  auto settings_widgets = ui->settings_frame->findChildren<QWidget*>(QRegularExpression{"setting_.*"});
  for_each(begin(settings_widgets), end(settings_widgets), bind(&QWidget::deleteLater, _1));
  scene->clear();
}

void QHYMainWindow::Private::enableUIWidgets(bool cameraConnected)
{
  ui->actionZoom_In->setEnabled(cameraConnected);
  ui->actionZoom_Out->setEnabled(cameraConnected);
  ui->actionActual_Size->setEnabled(cameraConnected);
  ui->actionFit_to_window->setEnabled(cameraConnected);
  ui->actionDisconnect->setEnabled(cameraConnected);
}

#include "qhymainwindow.moc"
