#include "pulseacquire.h"
#include "ui_pulseacquire.h"

#include <functional>

#include "misc.h"
#include "mainapplication.h"
#include "device.h"
#include "plots.h"

using namespace Task;

PulseAcquire::PulseAcquire(QWidget *parent, QString taskName,
                           App::MainApplication* const app, QPointer<Dev::DeviceManager> devMan) :
    ManagedTask(parent, taskName, TaskType::PulseAcquire),
    ui(new Ui::PulseAcquire),
    pulseSequenceCursorUpdateTimer(new QTimer),
    pulseSequence(new PulseSequence()),
    app(app),
    deviceManager(devMan),
    tempSettingsPath(app->getTempPath())
{
    ui->setupUi(this);

    ui->checkBox_pulse_script_run->setVisible(false);
    ui->lineEdit_pulse_script_path->setVisible(false);

    ui->checkBox_noTestSignal->setVisible(false);
    ui->groupBox->setVisible(true);
    ui->gridLayout->addWidget(ui->groupBox_pulseAcquireSettings, 0, 0, 2, 1);

    setTimeout(600000);

    runMode = RunMode::DevicesDisabled | RunMode::MenuDisabled;

    connect(this, &PulseAcquire::stateChanged,
            this, [=] () {ui->lineEdit->setText(QVariant::fromValue(getTaskState()).toString());});

    connect(ui->spinBox, &QSpinBox::editingFinished, this, [=] () {setTimeout(ui->spinBox->value()*1000);});
    connect(ui->pushButton_loadPulseSequence, &QPushButton::clicked, this, &PulseAcquire::loadPulseSequence);

    connect(deviceManager.data(), &Dev::DeviceManager::devicesChanged, this, &PulseAcquire::updateDevices);

    connect(ui->plotPulseSequence->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
            this, [=](const QCPRange &newRange)
            {if (newRange.lower < 0){ui->plotPulseSequence->xAxis->setRange(0, newRange.size()); return;}});

    connect(ui->checkBox_pulseSequenceCursor, &QCheckBox::clicked, this, &PulseAcquire::updatePulseSequenceCursor);

    connect(this, &Task::ManagedTask::timeOutChanged, this, [=] () {setTimeout(getTimeout());});

    timeDomainPlot = new Plot::TimeDomainPlot(this);
    freqDomainPlot = new Plot::FFTPlot(this);
    ui->widget_timePlot->layout()->addWidget(timeDomainPlot);
    ui->widget_freqPlot->layout()->addWidget(freqDomainPlot);

    setupPlots();

    auto topLayout = new QGridLayout();
    topLayout->addWidget(ui->groupBox_pulseAcquireSettings, 0, 0, 1, 1);
    topLayout->addWidget(ui->groupBox_2, 0, 1, 1, 1);
    auto topWidget = new QWidget(this);
    topWidget->setLayout(topLayout);

    auto bottomLayout = new QGridLayout();
    bottomLayout->addWidget(ui->widget_timePlot, 0, 0, 1, 1);
    bottomLayout->addWidget(ui->widget_freqPlot, 1, 0, 1, 1);
    auto bottomWidget = new QWidget(this);
    bottomWidget->setLayout(bottomLayout);

    auto splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(topWidget);
    splitter->addWidget(bottomWidget);

    ui->gridLayout->addWidget(splitter);
    for (auto i=0; i< ui->gridLayout->rowCount(); i ++){
        ui->gridLayout->setRowStretch(i, 0);
    }
}

PulseAcquire::~PulseAcquire(){
}

void PulseAcquire::saveSettings(QSettings &settings) const
{
    ManagedTask::saveSettings(settings);

    settings.setValue("NumberOfScans", ui->numberofScans->value());
    settings.setValue("Description", ui->lineEdit_pulseAcquireDescription->text());
    settings.setValue("RunScript", ui->checkBox_pulse_script_run->isChecked());
    settings.setValue("ScriptPath", ui->lineEdit_pulse_script_path->text());
    settings.setValue("DataDirectory", ui->lineEdit_pulseAcquireDataDirectory->text());
    settings.setValue("PulseSequenceCursor", ui->checkBox_pulseSequenceCursor->isChecked());
    settings.setValue("NoTestSignal", ui->checkBox_noTestSignal->isChecked());

    settings.setValue("PulseSequencePath", ui->lineEdit_pulseProgPath->text());

    settings.setValue("DAQ", ui->comboBox_daq->currentText());
    settings.setValue("AcquisitionMode", ui->comboBox_acquisitionMode->currentText());

    settings.beginGroup("Plots");
    freqDomainPlot->saveSettings(settings);
    timeDomainPlot->saveSettings(settings);
    settings.endGroup();
}

void PulseAcquire::loadSettings(QSettings &settings, const App::LoadOptions loadOptions)
{
    ManagedTask::loadSettings(settings, loadOptions);
    updateDevices();

    ui->numberofScans->setValue(settings.value("NumberOfScans", 1).toInt());
    ui->lineEdit_pulseAcquireDescription->setText(settings.value("Description").toString());
    ui->checkBox_pulse_script_run->setChecked(settings.value("RunScript", false).toBool());
    ui->lineEdit_pulse_script_path->setText(settings.value("ScriptPath").toString());
    ui->checkBox_pulseSequenceCursor->setChecked(settings.value("PulseSequenceCursor").toBool());

    ui->checkBox_noTestSignal->setChecked(settings.value("NoTestSignal").toBool());

    ui->lineEdit_pulseProgPath->setText(settings.value("PulseSequencePath").toString());

    ui->comboBox_daq->setCurrentText(settings.value("DAQ").toString());
    ui->comboBox_acquisitionMode->setCurrentText(settings.value("AcquisitionMode").toString());

    if (loadOptions & App::LoadOption::LoadPulseSequence){
        loadPulseSequence();
    }

    settings.beginGroup("Plots");
    freqDomainPlot->loadSettings(settings, loadOptions);
    timeDomainPlot->loadSettings(settings, loadOptions);
    settings.endGroup();

    QString oldPath = settings.value("DataDirectory").toString();
    QString newPath = oldPath;    

    if (loadOptions & App::LoadOption::ResumeTask){
        if (oldPath.indexOf("trash") == -1){
            QString cwd = app->getCurrentWorkingDirectory();
            QDir dataDirectory = getDataDirectory(oldPath, cwd);
            if (dataDirectory.exists()){
                newPath = incrementPathName(oldPath);
            }
        }
    }

    ui->lineEdit_pulseAcquireDataDirectory->setText(newPath);
}

void PulseAcquire::setDataDirectory(QString dataDirectory){
    ui->lineEdit_pulseAcquireDataDirectory->setText(dataDirectory);
}

void PulseAcquire::setNumberOfScans(int n){
    ui->numberofScans->setValue(n);
}

ManagedTask::RunState PulseAcquire::resume(){
    if(not isReadyToContinue()){
        return RunState::Idle;
    }

    if(counter >= counterMax){
        return RunState::Idle;
    }

    return requestData();
}

ManagedTask::RunState PulseAcquire::initialize(){
    disableGuiElements(runMode);

    app->saveSettings(tempSettingsPath);

    counterMax = ui->numberofScans->value();

    timeDomainPlot->reset();
    freqDomainPlot->reset();

    auto dataDirectoryString = ui->lineEdit_pulseAcquireDataDirectory->text();

    daq = deviceManager->getDevice<Dev::DAQ>(ui->comboBox_daq->currentText());

    // Create directory
    if (!(makeDataDirectory(dataDirectory, dataDirectoryString, app->getCurrentWorkingDirectory()))){
        return RunState::Failed;
    }

    if (!dataDirectory.isEmpty() && (dataDirectoryString.indexOf("trash") == -1)){
        if (QMessageBox::No == QMessageBox::question(
                    this, "Directory is not empty.", "Overwrite?", QMessageBox::Yes|QMessageBox::No)){
            return RunState::Aborted;
        }
    }

    QFileInfo pulseFile(ui->lineEdit_pulseProgPath->text());
    QFileInfo targetFile(dataDirectory.absoluteFilePath(pulseFile.fileName()));
    if (targetFile.exists()){
        QFile::remove(targetFile.absoluteFilePath());
    }

    if (not QFile(pulseFile.absoluteFilePath()).copy(targetFile.absoluteFilePath())){
        qWarning() << "Could not copy pulse file: " << targetFile.absoluteFilePath();
    }
    loadPulseSequence();

    return RunState::Idle;
}

ManagedTask::RunState PulseAcquire::finalize(){
    daq.clear();

    // Reset Settings
    if ((not tempSettingsPath.isEmpty()) and (QFile(tempSettingsPath).exists())){
        app->loadSettings(tempSettingsPath, App::LoadOption::NoOptions);
        QFile::remove(tempSettingsPath);
    }

    // Increment Directory Name
    auto oldDirectory = ui->lineEdit_pulseAcquireDataDirectory->text();
    if(oldDirectory.indexOf("trash") == -1){
        ui->lineEdit_pulseAcquireDataDirectory->setText(incrementPathName(oldDirectory));
    }

    // Enable Gui
    enableGuiElements();

    // Final clean-up
    if ((runState == RunState::Busy) and (not isTimedOut())){
        return RunState::Busy;
    }

    return RunState::Idle;
}

void PulseAcquire::changeEvent(QEvent *e)
{

    if (e->type() == QEvent::StyleChange)
    {
        QColor penColor = ui->plotPulseSequence->getColorFromPalette(0, 1, Plot::MyQCustomPlot::Palettes::Secondary);
        penColor.setAlpha(128);
#ifdef QCUSTOMPLOT_USE_OPENGL
        timeoutCursor->setPen(QPen(penColor, 4.0));
#else
        timeoutCursor->setPen(QPen(penColor, 1.0));
#endif

        updatePulseSequencePlot();
    }
    ManagedTask::changeEvent(e);
}

void PulseAcquire::updateDevices()
{
    auto currentText = ui->comboBox_daq->currentText();
    ui->comboBox_daq->clear();
    ui->comboBox_daq->addItems(deviceManager->getDeviceNames<Dev::DAQ>());
    ui->comboBox_daq->setCurrentText(currentText);
}

void PulseAcquire::on_comboBox_daq_currentTextChanged(QString)
{
    auto daq = deviceManager->getDevice<Dev::DAQ>(ui->comboBox_daq->currentText());
    if (daq.isNull()){
        ui->comboBox_acquisitionMode->clear();
        return;
    }

    auto currentText = ui->comboBox_acquisitionMode->currentText();
    ui->comboBox_acquisitionMode->clear();
    ui->comboBox_acquisitionMode->addItems(daq->getModes());
    ui->comboBox_acquisitionMode->setCurrentText(currentText);

    //if (daq->getModes().length() > 1){
    //    ui->groupBox_2->setEnabled(true);
    //} else {
    //    ui->groupBox_2->setEnabled(false);
    //}
}

void PulseAcquire::on_checkBox_pulse_script_run_stateChanged(int checkState)
{
    switch(checkState){
    case Qt::Checked:
        ui->lineEdit_pulse_script_path->setEnabled(true);
        break;
    case Qt::Unchecked:
        ui->lineEdit_pulse_script_path->setEnabled(false);
        break;
    default:
        break;
    }
}

void PulseAcquire::loadPulseSequence(){
    pulseSequence = QSharedPointer<PulseSequence>::create();

    QString pulseFilePath = ui->lineEdit_pulseProgPath->text();

    QStringList channels;

    //Load file
    QFile mFile(pulseFilePath);
    if(!mFile.open(QFile::ReadOnly | QFile::Text)){
        qCritical() <<  "Could not open file for reading";
        return;
    }

    QTextStream textStream(&mFile);
    QString textString;
    int numberOfPulses = 0;

    if (!textStream.atEnd()){
        textString = textStream.readLine();

        numberOfPulses = textString.split("#")[0].toInt();
        channels = textString.split("\t");
        channels.removeFirst();
    }

    int pulsesLoaded = 0;
    int pulseDuration;
    int timeStamp = 0;
    QList<QString> channelValues;
    QMap<QString, int32_t> channelData;
    while(!textStream.atEnd() and (pulsesLoaded < numberOfPulses)){
        textString = textStream.readLine();

        pulseDuration = textString.split("\t")[0].toInt() - timeStamp;
        timeStamp = textString.split("\t")[0].toInt();

        channelData.clear();
        for (int i=0; i<channels.length(); i++){
            channelData.insert(channels[i], channelValues.value(i).toInt());
        }

        pulseSequence->addPulse(pulseDuration, channelData);

        channelValues = textString.split("\t");
        channelValues.removeFirst(); // First is pulse duration
        pulsesLoaded++;
    }

    mFile.close();

    // Last pulse
    channelData.clear();
    for (int i=0; i<channels.length(); i++){
        channelData.insert(channels[i], channelValues.value(i).toInt());
    }
    pulseSequence->addPulse(5, channelData);

    if (!pulseSequence->isValid()){
        pulseSequence->clear();
        qCritical() << "Loaded pulse sequence is invalid.";
        return;
    }

    updatePulseSequencePlot();
}

void PulseAcquire::updatePulseSequenceCursor()
{
    if (not isRunning()) {
        return;
    }

    if (ui->checkBox_pulseSequenceCursor->isChecked()){
        pulseSequenceCursor->setVisible(true);
        timeoutCursor->setVisible(true);
    } else {
        auto wasVisible = pulseSequenceCursor->visible();
        pulseSequenceCursor->setVisible(false);
        timeoutCursor->setVisible(false);
        if (wasVisible){
            ui->plotPulseSequence->replot();
        }
        return;
    }

    if (pulseSequence.isNull()){
        return;
    }

    if (not pulseSequence->isValid()){
        pulseSequenceCursor->setVisible(false);
        timeoutCursor->setVisible(false);
        return;
    }

    auto elapsedMilliseconds = getElapsedMilliseconds();

    if (elapsedMilliseconds == -1){
        pulseSequenceCursor->point1->setCoords(0.0, 0.0);
        pulseSequenceCursor->point2->setCoords(0.0, 1.0);
        ui->plotPulseSequence->replot();
        if(isRunning() and (not isPaused())){
            pulseSequenceCursorUpdateTimer->singleShot(20, this, &PulseAcquire::updatePulseSequenceCursor);
        }
        return;
    }

    if ((getElapsedMilliseconds() + 100) > (totalPulseLength_us / 1000)){
        pulseSequenceCursor->point1->setCoords(totalPulseLength_us / 1000.0, 0.0);
        pulseSequenceCursor->point2->setCoords(totalPulseLength_us / 1000.0, 1.0);
        ui->plotPulseSequence->replot();
        pulseSequenceCursorUpdateTimer->singleShot(20, this, &PulseAcquire::updatePulseSequenceCursor);
        return;
    }

    pulseSequenceCursor->point1->setCoords(elapsedMilliseconds, 0.0);
    pulseSequenceCursor->point2->setCoords(elapsedMilliseconds, 1.0);
    ui->plotPulseSequence->replot();

    pulseSequenceCursorUpdateTimer->singleShot(20, this, &PulseAcquire::updatePulseSequenceCursor);
}

void PulseAcquire::on_pushButton_openSequence_clicked()
{
    auto currentParent = QDir(ui->lineEdit_pulseProgPath->text());
    currentParent.cdUp();
    auto fileName = QFileDialog::getOpenFileName(
        this,
        "Open Pulse Sequence File",
        currentParent.absolutePath(),
        "");

    if (not QFile::exists(fileName)){
        return;
    }

    ui->lineEdit_pulseProgPath->setText(fileName);
    loadPulseSequence();
}

void PulseAcquire::prepareTask()
{
}

ManagedTask::RunState PulseAcquire::requestData(){
    daq->setMode(ui->comboBox_acquisitionMode->currentText());

    switch(daq->uploadPulseSequence(pulseSequence)){
    case 1:
        pulseSequence->clear();
        break;
    case -1:
        return RunState::Failed;
    default:
        break;
    }

    if(daq->acquireData(std::bind(&PulseAcquire::receiveData, this, std::placeholders::_1))){
        qCritical() << "Cannot acquire Data.";
        return RunState::Failed;
    }

    updatePulseSequenceCursor();
    return RunState::Busy;
}

void PulseAcquire::receiveData(int error){
    if(not isReadyToContinue()){
        emit alive(RunState::Idle);
        return;
    }

    if (error == 0){
        processData();
    } else {
        qCritical() << "Acquisiton failed.";
    }

    emit alive(resume());
    return;
}

void PulseAcquire::processData(){
    auto raw16bitData = daq->get16BitBuffer();
    auto raw32bitData = daq->get32BitBuffer();

    if (not raw16bitData.isEmpty()){
        dataWriteToFile(dataDirectory.absoluteFilePath(QString::number(counter) + ".dat"), raw16bitData);
    } else {
        dataWriteToFile(dataDirectory.absoluteFilePath(QString::number(counter) + ".dat"), raw32bitData);
    }

    auto plotData = daq->getPlotData();
    timeDomainPlot->updateData(plotData);
    freqDomainPlot->updateData(plotData);

    app->saveSettings(dataDirectory.absoluteFilePath(QString::number(counter) + ".ini"));
    QSettings settings(dataDirectory.absoluteFilePath(QString::number(counter) + ".ini"), QSettings::IniFormat);

    counter++;

    settings.setValue("ActiveTask", name);
    settings.sync();
}

void PulseAcquire::setupPlots(){    
    pulseSequenceCursor = new QCPItemStraightLine(ui->plotPulseSequence);
    pulseSequenceCursor->point1->setCoords(0.0, 0.0);
    pulseSequenceCursor->point2->setCoords(0.0, 1.0);
#ifdef QCUSTOMPLOT_USE_OPENGL
    pulseSequenceCursor->setPen(QPen(QColor(255, 255, 255, 128), 4.0));
#else
    pulseSequenceCursor->setPen(QPen(QColor(255, 255, 255, 128), 1.0));
#endif

    timeoutCursor = new QCPItemStraightLine(ui->plotPulseSequence);
    timeoutCursor->point1->setCoords(getTimeout(), 0.0);
    timeoutCursor->point2->setCoords(getTimeout(), 1.0);
    QColor penColor = ui->plotPulseSequence->getColorFromPalette(0, 1, Plot::MyQCustomPlot::Palettes::Secondary);
    penColor.setAlpha(128);
#ifdef QCUSTOMPLOT_USE_OPENGL
    timeoutCursor->setPen(QPen(penColor, 4.0));
#else
    timeoutCursor->setPen(QPen(penColor, 1.0));
#endif

    ui->plotPulseSequence->xAxis->setLabel("Time (ms)");
    ui->plotPulseSequence->yAxis->setLabel("Output ()");
    ui->plotPulseSequence->xAxis->setRange(0, 1000000);
    ui->plotPulseSequence->yAxis->setRange(-0.1, 5*1.5 + 1.1);
    ui->plotPulseSequence->yAxis->setTicks(false);
    ui->plotPulseSequence->xAxis->setNumberFormat("f");
    ui->plotPulseSequence->xAxis->setNumberPrecision(0);
    ui->plotPulseSequence->xAxis2->setVisible(true);
    ui->plotPulseSequence->yAxis2->setVisible(true);
    ui->plotPulseSequence->xAxis2->setTicks(false);
    ui->plotPulseSequence->yAxis2->setTicks(false);

    ui->plotPulseSequence->legend->setVisible(true);
    ui->plotPulseSequence->yAxis->grid()->setPen(Qt::NoPen);

    ui->plotPulseSequence->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);// | QCP::iSelectAxes);
    ui->plotPulseSequence->axisRect()->setRangeDrag(Qt::Horizontal);
    ui->plotPulseSequence->axisRect()->setRangeZoom(Qt::Horizontal);

    QCPLayoutGrid *subLayout = new QCPLayoutGrid;
    ui->plotPulseSequence->plotLayout()->addElement(0, 1, subLayout);
    subLayout->setMargins(QMargins(0, 0, 0, 0));
    subLayout->addElement(0, 0, ui->plotPulseSequence->legend);
    ui->plotPulseSequence->legend->setFillOrder(QCPLegend::foRowsFirst);
    ui->plotPulseSequence->legend->setBorderPen(QPen(Qt::NoPen));
    ui->plotPulseSequence->plotLayout()->setColumnStretchFactor(1, 0.001);

    auto plotColor = ui->plotPulseSequence->getColorFromPalette(0, 1);
    ui->plotPulseSequence->addGraph();

#ifdef QCUSTOMPLOT_USE_OPENGL
    ui->plotPulseSequence->graph(0)->setPen(QPen(plotColor, 4));
#else
    ui->plotPulseSequence->graph(0)->setPen(QPen(plotColor, 1));
#endif

    ui->plotPulseSequence->replot();
}

void PulseAcquire::updatePulseSequencePlot(){
    int n = pulseSequence->getNumberOfPulses();
    int pulseDuration;
    int timePassed = 0;
    int32_t channelValue, maxChannelValue;

    QVector<double> xData, yData;
    xData.resize(n);//+2);
    yData.resize(n);//+2);

    ui->plotPulseSequence->clearGraphs();
    ui->plotPulseSequence->legend->clear();

    auto numberOfChannels = pulseSequence->getChannels().length();
    for(int i=0; i < numberOfChannels; i++){
        auto channel = pulseSequence->getChannels()[i];
        timePassed = 0;

        auto offset = 2.5*(numberOfChannels - i - 1);
        xData[0] = 0;
        yData[0] = offset;

        maxChannelValue = 1;
        for (int j=0; j < n; j++){
            maxChannelValue = std::max(maxChannelValue,
                                       static_cast<int32_t>(std::abs(pulseSequence->getChannelData(j, channel))));
        }

        for (int j=0; j < n - 1; j++){
            pulseDuration = pulseSequence->getDuration(j);
            channelValue = pulseSequence->getChannelData(j, channel);

            timePassed += pulseDuration;

            xData[j+1] = timePassed/1000.0;
            yData[j+1] = static_cast<double>(channelValue)/maxChannelValue + offset;
        }

        ui->plotPulseSequence->addGraph();
#ifdef QCUSTOMPLOT_USE_OPENGL
        ui->plotPulseSequence->graph(2*i)->setPen(QPen(QColor(192, 192, 192, 255), 2.0));
#else
        ui->plotPulseSequence->graph(2*i)->setPen(QPen(QColor(192, 192, 192, 255), 1.0));
#endif
        ui->plotPulseSequence->graph(2*i)->setData({xData[0], xData[n-1]}, {offset, offset});
        ui->plotPulseSequence->graph(2*i)->removeFromLegend();

        QColor plotColor = ui->plotPulseSequence->getColorFromPalette(i, numberOfChannels+1);
        ui->plotPulseSequence->addGraph();
#ifdef QCUSTOMPLOT_USE_OPENGL
        ui->plotPulseSequence->graph(2*i+1)->setPen(QPen(plotColor, 4));
#else
        ui->plotPulseSequence->graph(2*i+1)->setPen(QPen(plotColor, 1));
#endif
        ui->plotPulseSequence->graph(2*i+1)->setLineStyle(QCPGraph::lsStepRight);
        ui->plotPulseSequence->graph(2*i+1)->setData(xData, yData);
        ui->plotPulseSequence->graph(2*i+1)->setName(channel);
    }

    totalPulseLength_us = timePassed;

    ui->plotPulseSequence->xAxis->setRange(0, timePassed/1000.0);
    ui->plotPulseSequence->yAxis->setRange(-1.25, (numberOfChannels-1) * 2.5 + 1.25);
    ui->plotPulseSequence->replot();
}

void PulseAcquire::setTimeout(int msecs){
    ManagedTask::setTimeout(msecs);
    ui->spinBox->setValue(msecs/1000);
    if (not timeoutCursor.isNull()){
        timeoutCursor->point1->setCoords(msecs, 0.0);
        timeoutCursor->point2->setCoords(msecs, 1.0);
        ui->plotPulseSequence->replot();
    }
}

void PulseAcquire::enableGuiElements(){
    ui->groupBox->setEnabled(true);
    ui->groupBox_pulseAcquireSettings->setEnabled(true);
}

void PulseAcquire::disableGuiElements(RunModes){
    ui->groupBox->setEnabled(false);
    ui->groupBox_pulseAcquireSettings->setEnabled(false);
}
