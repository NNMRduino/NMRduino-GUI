#ifndef PULSEACQUIRE_H
#define PULSEACQUIRE_H

#include "tasks.h"
#include "device.h"

#include <QDir>
#include <QMap>

class QSpinBox;

namespace App{
    class MainApplication;
}

namespace Task{
    class PulseAcquire;
}

namespace Ui{
    class PulseAcquire;
}

namespace Plot{
    class TimeDomainPlot;
    class FFTPlot;
}

class PulseSequence;
class QCPItemStraightLine;

class Task::PulseAcquire : public Task::ManagedTask
{
   Q_OBJECT
public:
    explicit PulseAcquire(QWidget *parent=nullptr, QString taskName="",
                          App::MainApplication* const app=nullptr, QPointer<Dev::DeviceManager> devManager=nullptr);
    ~PulseAcquire() override;

    void saveSettings(QSettings &settings) const override;
    void loadSettings(QSettings &, const App::LoadOptions) override;

    void setDataDirectory(QString dataDirectory) override;
    void setNumberOfScans(int n);

protected:
    RunState resume() override;
    RunState initialize() override;
    RunState finalize() override;

    virtual void changeEvent(QEvent *e) override;

public slots:
    void updateDevices();

private slots:
    void on_comboBox_daq_currentTextChanged(QString);
    void on_checkBox_pulse_script_run_stateChanged(int state);
    void loadPulseSequence();
    void updatePulseSequenceCursor();
    void on_pushButton_openSequence_clicked();

private:
    void prepareTask() override;

    ManagedTask::RunState requestData();
    void receiveData(const int error);
    void processData();
    void nextScan();

    void setupPlots();
    void updatePulseSequencePlot();

    void setTimeout(int msecs);

    void enableGuiElements() override;
    void disableGuiElements(RunModes) override;

    QVector<quint16> rawData;
    QVector<double> timeData, voltageData;
    QVector<double> amplData, freqData;
    int outOfRange;

    QScopedPointer<Ui::PulseAcquire> ui;
    QPointer<QCPItemStraightLine> pulseSequenceCursor, timeoutCursor;
    QSharedPointer<QTimer> pulseSequenceCursorUpdateTimer;

    QPointer<Plot::TimeDomainPlot> timeDomainPlot;
    QPointer<Plot::FFTPlot> freqDomainPlot;

    QSharedPointer<PulseSequence> pulseSequence;
    int totalPulseLength_us;
    QPointer<App::MainApplication> app;
    QPointer<Dev::DeviceManager> deviceManager;
    QSharedPointer<Dev::DAQ> daq;

    QMetaObject::Connection receiveConnection, failedConnection;

    QDir dataDirectory;

    //QList<QPen> pulsePlotPens;

    const QString tempSettingsPath;
};

#endif // PULSEACQUIRE_H
