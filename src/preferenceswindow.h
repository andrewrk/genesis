#ifndef PREFERENCESWINDOW_H
#define PREFERENCESWINDOW_H

#include <QDialog>

namespace Ui {
    class PreferencesWindow;
}

class PreferencesWindow : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesWindow(QWidget *parent = 0);
    ~PreferencesWindow();

private:
    Ui::PreferencesWindow *ui;

    void hideErr();
    void showErr(QString msg);

private slots:
    void refreshAudioBackends();
    void on_refreshBackendsBtn_clicked();
    void on_backendDropdown_currentIndexChanged(int index);
    void on_latencySlider_valueChanged(int value);
    void on_latencySlider_sliderMoved(int position);
};

#endif // PREFERENCESWINDOW_H
