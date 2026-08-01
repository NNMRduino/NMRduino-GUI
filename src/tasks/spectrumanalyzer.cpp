#include "spectrumanalyzer.h"
#include "plots/timedomainplot.h"
#include "ui_spectrumanalyzer.h"

#include <QTimer>
#include <QDebug>
#include <QSettings>
#include <QFile>

#include "mainapplication.h"
#include "device.h"
#include "plots.h"

using namespace Task;

SpectrumAnalyzer::SpectrumAnalyzer(QWidget *parent, QString taskName,
                                   App::MainApplication* const app, QPointer<Dev::DeviceManager> devMan):
    ManagedTask(parent, taskName, TaskType::SpectrumAnalyzer),
    ui(new Ui::SpectrumAnalyzer),
    app(app),
    deviceManager(devMan),
    tempSettings(app->getTempPath()),
    plotUpdateTimer(new QTimer())
{
    ui->setupUi(this);

    ui->spinBox_supersampling->setVisible(false);
    ui->label_7->setVisible(false);
    ui->groupBox->setVisible(false);
    ui->gridLayout->addWidget(ui->groupBox_2, 0, 0, 1, 2);

    runMode  = RunMode::MenuDisabled | RunMode::DAQDisabled;

    setTimeout(600000);

    dataLength = 8192;
    //sampleRate = 2000.0;

    refreshRate = 40; // 25 Hz
    plotUpdateTimer->setInterval(refreshRate);
    connect(plotUpdateTimer.data(), &QTimer::timeout, this, &SpectrumAnalyzer::processUnbufferedData);

    ui->comboBox_mode->clear();
    ui->comboBox_mode->addItem("Buffered");
    ui->comboBox_mode->addItem("Unbuffered");

    connect(ui->spinBox_timeOut, &QSpinBox::editingFinished, this, [=] () {setTimeout(ui->spinBox_timeOut->value()*1000);});
    connect(this, &SpectrumAnalyzer::stateChanged,
            this, [=] () {ui->lineEdit_state->setText(getTaskState());});

    connect(devMan.data(), &Dev::DeviceManager::devicesChanged, this, &SpectrumAnalyzer::updateDevices);

    connect(this, &Task::ManagedTask::timeOutChanged, this, [=] () {setTimeout(getTimeout());});

    timeDomainPlot = new Plot::TimeDomainPlot(this);
    freqDomainPlot = new Plot::FFTPlot(this);
    ui->widget_timePlot->layout()->addWidget(timeDomainPlot);
    ui->widget_ftPlot->layout()->addWidget(freqDomainPlot);
}

SpectrumAnalyzer::~SpectrumAnalyzer(){
}

void SpectrumAnalyzer::saveSettings(QSettings &settings) const{
    ManagedTask::saveSettings(settings);

    timeDomainPlot->saveSettings(settings);
    freqDomainPlot->saveSettings(settings);

    settings.setValue("SampleRate", ui->doubleSpinBox_sampleRate->value());
    settings.setValue("Datalength", ui->comboBox_dataLength->currentText());
    settings.setValue("Supersampling", ui->spinBox_supersampling->value());
    settings.setValue("DaqDevice", ui->comboBox_daq->currentText());
    settings.setValue("Mode", ui->comboBox_mode->currentText());
}

void SpectrumAnalyzer::loadSettings(QSettings &settings, const App::LoadOptions loadOptions){
    ManagedTask::loadSettings(settings, loadOptions);
    updateDevices();

    timeDomainPlot->loadSettings(settings, loadOptions);
    freqDomainPlot->loadSettings(settings, loadOptions);

    ui->doubleSpinBox_sampleRate->setValue(settings.value("SampleRate", 2000.0).toInt());
    ui->comboBox_dataLength->setCurrentText(settings.value("Datalength", 2048).toString());
    ui->spinBox_supersampling->setValue(settings.value("Supersampling", 0).toInt());
    ui->comboBox_daq->setCurrentText(settings.value("DaqDevice").toString());
    ui->comboBox_mode->setCurrentText(settings.value("Mode", "Unbuffered").toString());
}

void SpectrumAnalyzer::setTimeout(int msecs) {
    ManagedTask::setTimeout(msecs);
    ui->spinBox_timeOut->setValue(msecs/1000);
}

void SpectrumAnalyzer::enableGuiElements(){
    ui->groupBox->setEnabled(true);
}

void SpectrumAnalyzer::disableGuiElements(RunModes){
    ui->groupBox->setEnabled(false);
}

ManagedTask::RunState SpectrumAnalyzer::resume(){
    if(not isReadyToContinue()){
        return RunState::Idle;
    }

    return requestData();
}

ManagedTask::RunState SpectrumAnalyzer::initialize(){
    disableGuiElements(runMode);

    QSettings settings(tempSettings, QSettings::IniFormat);

    daq = deviceManager->getDevice<Dev::DAQ>(ui->comboBox_daq->currentText());
    daq.dynamicCast<Dev::GeneralDevice>()->saveSettings(settings);

    timeDomainPlot->reset();
    freqDomainPlot->reset();

    return RunState::Idle;
}

ManagedTask::RunState SpectrumAnalyzer::finalize()
{
    // Reset Settings
    if (not tempSettings.isEmpty() and QFile(tempSettings).exists()){
        QSettings settings(tempSettings, QSettings::IniFormat);
        daq.dynamicCast<Dev::GeneralDevice>()->loadSettings(settings, App::LoadOption::NoOptions);
        QFile(tempSettings).remove();
    }

    // Enable Gui
    enableGuiElements();

    plotUpdateTimer->stop();

    if ((runState == RunState::Busy) and (mode == Dev::DAQ::Mode::Unbuffered)){
        daq->abortAcquisition();
    }

    daq.clear();

    if (mode == Dev::DAQ::Mode::Unbuffered){
        return RunState::Idle;
    }

    // Final clean-up
    if ((runState == RunState::Busy) and (not isTimedOut())){
        return RunState::Busy;
    }

    return RunState::Idle;
}

void SpectrumAnalyzer::prepareTask()
{
}

ManagedTask::RunState SpectrumAnalyzer::requestData(){
    auto modeString = ui->comboBox_mode->currentText();

    if(modeString == "Buffered"){
        mode = Dev::DAQ::Mode::Buffered;
    } else if (modeString == "Unbuffered"){
        mode = Dev::DAQ::Mode::Unbuffered;
    } else {
        mode = Dev::DAQ::Mode::None;
    }

    daq->setMode(mode);
    daq->setNumberOfSamples(ui->comboBox_dataLength->currentData().toInt());
    daq->setSampleRate(ui->doubleSpinBox_sampleRate->value());
    daq->setSupersamplingFactor(ui->spinBox_supersampling->value());

    dataLength = daq->getNumberOfSamples();
    //sampleRate = daq->getSampleRate();

    ui->comboBox_dataLength->setCurrentIndex(ui->comboBox_dataLength->findData(dataLength));
    ui->doubleSpinBox_sampleRate->setValue(daq->getSampleRate());

    int err = -1;
    switch(mode){
    case Dev::DAQ::Mode::None:
    case Dev::DAQ::Mode::Buffered:
        err = daq->acquireData(std::bind(&SpectrumAnalyzer::receiveData, this, std::placeholders::_1));
        break;
    case Dev::DAQ::Mode::Unbuffered:
        err = daq->acquireData();
        plotUpdateTimer->start();
        break;
    }

    if (err != 0){
        return RunState::Failed;
    }

    return RunState::Busy;
}

void SpectrumAnalyzer::receiveData(int err){
    if(not isReadyToContinue()){
        emit alive(RunState::Idle);
        return;
    }

    if (err == 0){
        processData();
    } else {
        qCritical() << "Acquisiton failed.";
    }

    emit alive(resume());
    return;
}

void SpectrumAnalyzer::processData(){
    auto plotData = daq->getPlotData();

    timeDomainPlot->updateData(plotData);
    freqDomainPlot->updateData(plotData);
}

void SpectrumAnalyzer::processUnbufferedData(){
    if(not isReadyToContinue()){
        return;
    }

    auto plotData = daq->getPlotData(dataLength/2, true);
    timeDomainPlot->updateData(plotData);
    freqDomainPlot->updateData(plotData);

    emit alive(RunState::Busy);
}

void SpectrumAnalyzer::updateDevices(){
    auto currentText = ui->comboBox_daq->currentText();
    ui->comboBox_daq->clear();
    ui->comboBox_daq->addItems(deviceManager->getDeviceNames<Dev::DAQ>());
    ui->comboBox_daq->setCurrentText(currentText);
}

void SpectrumAnalyzer::on_comboBox_daq_currentTextChanged(QString){
    auto daq = deviceManager->getDevice<Dev::DAQ>(ui->comboBox_daq->currentText());
    if (daq.isNull()){
        ui->comboBox_dataLength->clear();
        return;
    }

    auto currentText = ui->comboBox_dataLength->currentText();
    ui->comboBox_dataLength->clear();
    foreach(auto dataLengthItem, daq->getAllowedNumberOfSamples()){
        ui->comboBox_dataLength->addItem(QString::number(dataLengthItem), dataLengthItem);
    }

    ui->comboBox_dataLength->setCurrentText(currentText);
}

