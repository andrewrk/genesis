#include "preferenceswindow.h"
#include "ui_preferenceswindow.h"

#include "playbackmodule.h"

#include <QTimer>

PreferencesWindow::PreferencesWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesWindow)
{
    ui->setupUi(this);

    hideErr();

    int milliseconds = PlaybackModule::latency() * 1000;
    ui->latencySlider->setValue(milliseconds);
    on_latencySlider_sliderMoved(milliseconds);
}

PreferencesWindow::~PreferencesWindow()
{
    delete ui;
}

void PreferencesWindow::hideErr()
{
    ui->backendErrLabel->hide();
}

void PreferencesWindow::showErr(QString msg)
{
    ui->backendErrLabel->setText(msg);
    ui->backendErrLabel->show();
}

void PreferencesWindow::on_latencySlider_valueChanged(int value)
{
    on_latencySlider_sliderMoved(value);
    int seconds = value / 1000;
    int err = PlaybackModule::setLatency(seconds);
    if (err != 0) {
        showErr(tr("Unable to set latency: %1").arg(PlaybackModule::errorString(err)));
    }
}

void PreferencesWindow::on_latencySlider_sliderMoved(int position)
{
    ui->latencyValueLabel->setText(tr("%1 ms").arg(QString::number(position)));
}
