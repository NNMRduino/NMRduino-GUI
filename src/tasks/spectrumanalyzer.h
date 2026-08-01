#ifndef SPECTRUMANALYZER_H
#define SPECTRUMANALYZER_H

#include <QObject>
#include <QThread>

#include "tasks.h"
#include "device.h"

namespace App {
    enum class PulseProgram;
}
namespace Task{
    class SpectrumAnalyzer;
}

namespace Dev{
    class DeviceManager;
}

namespace Plot{
    class TimeDomainPlot;
    class FFTPlot;
}


namespace Ui {
    class SpectrumAnalyzer;
}

class Task::SpectrumAnalyzer : public Task::ManagedTask
{
    Q_OBJECT
public:
    explicit SpectrumAnalyzer(QWidget *parent, QString taskName,
                              App::MainApplication* const app, QPointer<Dev::DeviceManager> devManager);
    virtual ~SpectrumAnalyzer() override;

    void saveSettings(QSettings &settings) const override;
    void loadSettings(QSettings &, const App::LoadOptions) override;

public slots:    
    void updateDevices();

private slots:
    void on_comboBox_daq_currentTextChanged(QString);

protected:
    RunState resume() override;
    RunState initialize() override;
    RunState finalize() override;

    void prepareTask() override;

private:
    ManagedTask::RunState requestData();
    void receiveData(int err);
    void processData();
    void processUnbufferedData();

    void setTimeout(int msecs);

    void enableGuiElements() override;
    void disableGuiElements(RunModes) override;

    //double sampleRate;
    int dataLength;

    QVector<quint16> rawData;
    QVector<double> timeData, voltageData;
    QVector<double> timeDataFiltered, voltageDataFiltered;
    QVector<double> amplData, freqData;

    QScopedPointer<Ui::SpectrumAnalyzer> ui;
    QPointer<Plot::TimeDomainPlot> timeDomainPlot;
    QPointer<Plot::FFTPlot> freqDomainPlot;
    QPointer<App::MainApplication> app;
    QPointer<Dev::DeviceManager> deviceManager;

    QString tempSettings;

    QScopedPointer<QTimer> plotUpdateTimer;
    Dev::DAQ::Mode mode;
    QSharedPointer<Dev::DAQ> daq;
    int refreshRate;

    QThread plotDataHandlerThread;
};

#endif // SPECTRUMANALYZER_H
