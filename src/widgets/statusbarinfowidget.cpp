/*
 * Copyright (C) 2016  Marco Gulino <marco@gulinux.net>
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

#include "statusbarinfowidget.h"
#include "ui_statusbarinfowidget.h"
#include <QDebug>

using namespace std;

class StatusBarInfoWidget::Private {
public:
  Private(StatusBarInfoWidget *q);
  unique_ptr<Ui::StatusBarInfoWidget> ui;
  
private:
  StatusBarInfoWidget *q;
};

StatusBarInfoWidget::Private::Private(StatusBarInfoWidget* q) : ui{new Ui::StatusBarInfoWidget}, q{q}
{

}


StatusBarInfoWidget::~StatusBarInfoWidget()
{

}

StatusBarInfoWidget::StatusBarInfoWidget(QWidget* parent, Qt::WindowFlags f) : QWidget(parent, f), dptr(this)
{
    d->ui->setupUi(this);
    captureFPS(0);
    displayFPS(0);
}

void StatusBarInfoWidget::captureFPS(double fps)
{
  d->ui->capture_fps_frame->setHidden(fps <= 0);
  d->ui->capture_fps->setText(QString::number(fps, 'f', 2));
}

void StatusBarInfoWidget::displayFPS(double fps)
{
  d->ui->display_fps_frame->setHidden(fps <= 0);
  d->ui->display_fps->setText(QString::number(fps, 'f', 2));
}


void StatusBarInfoWidget::deviceConnected(const QString& name)
{
  d->ui->device_status->setText(tr("Connected to %1").arg(name));
}

void StatusBarInfoWidget::deviceDisconnected()
{
  d->ui->device_status->setText(tr("Disconnected"));
}


#include "statusbarinfowidget.moc"