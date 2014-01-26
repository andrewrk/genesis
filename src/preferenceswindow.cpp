#include "preferenceswindow.h"
#include "ui_preferenceswindow.h"

#include "soundengine.h"

#include <QTimer>

PreferencesWindow::PreferencesWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PreferencesWindow)
{
    ui->setupUi(this);

    hideErr();

    refreshAudioBackends();

    int milliseconds = SoundEngine::latency() * 1000;
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

void PreferencesWindow::refreshAudioBackends()
{
    SoundEngine::refreshBackends();
    ui->backendDropdown->clear();
    ui->backendDropdown->addItems(SoundEngine::backends());
    ui->backendDropdown->setCurrentIndex(SoundEngine::selectedBackendIndex());
}

void PreferencesWindow::on_refreshBackendsBtn_clicked()
{
    refreshAudioBackends();
}

void PreferencesWindow::on_backendDropdown_currentIndexChanged(int index)
{
    int err = SoundEngine::setSoundParams(index, SoundEngine::sampleRate(), SoundEngine::channelLayout(), SoundEngine::latency());
    if (err != 0) {
        showErr(tr("Unable to set backend: %1").arg(SoundEngine::errorString(err)));
        refreshAudioBackends();
    }
}

void PreferencesWindow::on_latencySlider_valueChanged(int value)
{
    on_latencySlider_sliderMoved(value);
    int seconds = value / 1000;
    int err = SoundEngine::setSoundParams(SoundEngine::selectedBackendIndex(), SoundEngine::sampleRate(), SoundEngine::channelLayout(), seconds);
    if (err != 0) {
        showErr(tr("Unable to set latency: %1").arg(SoundEngine::errorString(err)));
        refreshAudioBackends();
    }
}

void PreferencesWindow::on_latencySlider_sliderMoved(int position)
{
    double seconds = position / 1000.0;
    int frames = SoundEngine::sampleRate() * seconds;
    ui->latencyValueLabel->setText(tr("%1 ms (%2 frames)").arg(QString::number(position), QString::number(frames)));
}
