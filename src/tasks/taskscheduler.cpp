#include "taskscheduler.h"
#include "ui_taskscheduler.h"

#include <QSettings>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QFileDialog>

#include "misc.h"
#include "mainapplication.h"
#include "device.h"

using namespace Task;

TaskScheduler::TaskScheduler(QWidget *parent, QString taskName,
                           App::MainApplication* const app, TaskManager * const taskManager, TaskQueue * const taskQueue) :
    ManagedTask(parent, taskName, TaskType::TaskScheduler),
    ui(new Ui::TaskScheduler),
    app(app),
    taskManager(taskManager),
    taskQueue(taskQueue),
    tempSettingsPath(app->getTempPath()),
    updateTimer(new QTimer)
{
    ui->setupUi(this);

    ui->groupBox->setVisible(true);
    //ui->gridLayout->addWidget(ui->groupBox_pulseAcquireListSettings, 0, 0, 1, 2);

    setTimeout(600000);

    runMode = RunMode::DevicesDisabled | RunMode::MenuDisabled;

    connect(ui->spinBox, &QSpinBox::editingFinished, this, [=] () {setTimeout(ui->spinBox->value()*1000);});

    connect(this, &TaskScheduler::stateChanged,
            this, [=] () {ui->lineEdit->setText(QVariant::fromValue(getTaskState()).toString());});

    connect(taskManager, &Task::TaskManager::tasksChanged, this, &TaskScheduler::updateTasks);
    connect(ui->pushButton_loadParameters, &QPushButton::clicked, this, &TaskScheduler::loadParameters);
    connect(this, &Task::ManagedTask::timeOutChanged, this, [=] () {setTimeout(getTimeout());});

    updateTimer->setInterval(50);
    connect(updateTimer.data(), &QTimer::timeout, this, &TaskScheduler::updateStatus);
    connect(this, &Task::ManagedTask::timedOut, this, &TaskScheduler::subTaskFinished);
}

TaskScheduler::~TaskScheduler(){
}

void TaskScheduler::loadNewParameters(){
    QSettings qSettings(tempConfigPath, QSettings::IniFormat);
    app->saveSettings(qSettings);

    auto loadOptions = App::LoadOptions(App::LoadOption::NoOptions);

    qDebug() << "Setting new parameters...";
    for (auto i=0; i<parameterNames.length(); i++){
        auto parameter = parameterNames.value(i);
        auto value = parameters.value(counter).value(i);

        //qDebug() << parameter << value;
        if(parameter.isEmpty() or value.isEmpty()){
            continue;
        }

        bool ok;
        value.toDouble(&ok);
        if (ok){
            qSettings.setValue(parameter, value.toDouble());
        } else {
            qSettings.setValue(parameter, value);
        }

        qDebug() << parameter << "set to" << value;

        if (parameter.contains("LarmorFrequency")){
            app->loadSettings(qSettings, App::LoadOption::SetLarmorFrequency);
            app->saveSettings(qSettings);
        } else if (parameter.contains("PulseSequencePath")){ // ToDo: Is this necessary?
            loadOptions |= App::LoadOption::LoadPulseSequence;
        }
    }

    qDebug() << "Done.";

    app->loadSettings(qSettings, loadOptions);
}

void TaskScheduler::movePreviousFiles(){
    dataDirectory.mkdir(QString::number(counter));

    if (not (tempDataDirectory.path().endsWith("Data") and tempDataDirectory.path().contains("Temp"))){
        QMessageBox::warning(this, "Warning!!!", "Tried to delete: " + tempDataDirectory.path());
        return;
    }

    tempDataDirectory.refresh();
    foreach(QString file, tempDataDirectory.entryList()){
        auto newFileName = QString("%1%2%3%2%4").arg(dataDirectory.absolutePath(), QDir::separator(),
                                                     QString::number(counter), file);
        if (file == "." or file ==".."){
            continue;
        }
        if (dataDirectory.exists(newFileName)){
            dataDirectory.remove(newFileName);
        }
        tempDataDirectory.rename(file, newFileName);
        qDebug() << file << newFileName;
    }

    if (tempDataDirectory.path().endsWith("Data") and tempDataDirectory.path().contains("Temp")){
        tempDataDirectory.removeRecursively(); // Very dangerous!!!

    } else {
        QMessageBox::warning(this, "Warning!!!", "Tried to delete: " + tempDataDirectory.path());
    }
}

void TaskScheduler::saveSettings(QSettings &settings) const{
    settings.setValue("DatFilePath", ui->lineEdit_relaxFile->text());
    settings.setValue("DataDirectory", ui->lineEdit_relaxData->text());
    settings.setValue("SubtaskName", ui->comboBox_paTasks->currentText());
    settings.setValue("Description", ui->lineEdit_description->text());

    ManagedTask::saveSettings(settings);
}

void TaskScheduler::loadSettings(QSettings &settings, const App::LoadOptions loadOptions){
    ManagedTask::loadSettings(settings, loadOptions);

    ui->lineEdit_relaxFile->setText(settings.value("DatFilePath").toString());
    ui->lineEdit_relaxData->setText(settings.value("DataDirectory").toString());
    ui->comboBox_paTasks->setCurrentText(settings.value("SubtaskName").toString());
    ui->lineEdit_description->setText(settings.value("Description").toString());

    if (loadOptions & App::LoadOption::ResumeTask){
        QString cwd = app->getCurrentWorkingDirectory();
        QDir dataDirectory = getDataDirectory(ui->lineEdit_relaxData->text(), cwd);
        if (dataDirectory.exists()){
            ui->lineEdit_relaxData->setText(incrementPathName(ui->lineEdit_relaxData->text()));
        }
    }
}



ManagedTask::RunState TaskScheduler::resume(){
    if (not isReadyToContinue()){
        return RunState::Idle;
    }

    if (taskSubmitted){
        movePreviousFiles();
        counter++;
        taskSubmitted=false;
        parametersLoaded=false;
    }

    if (counter >= counterMax){
        return RunState::Idle;
    }

    if (not parametersLoaded){
        loadNewParameters();
        parametersLoaded=true;
    }

    if (priority == Priority::High){
        qCritical() << "Error: Task already has the highest priority.";
        return RunState::Failed;
    }

    auto task = taskManager->getTask(ui->comboBox_paTasks->currentText());
    if(task.isNull()){
        qCritical() << "Error: Selected Task does not exists.";
       return RunState::Failed;
    }

    if (tempDataDirectory.path().endsWith("Data") and tempDataDirectory.path().contains("Temp")){
        tempDataDirectory.removeRecursively(); // Very dangerous!!!

    } else {
        QMessageBox::warning(this, "Warning!!!", "Tried to delete: " + tempDataDirectory.path());
        return RunState::Failed;
    }

    task->setDataDirectory(tempDataDirectory.path());

    task->priority = static_cast<Priority>(static_cast<int>(priority) + 1);
    task->resetTask();

    submittedTask = task;
    taskQueue->submitTaskToQueue(submittedTask);
    taskSubmitted = true;
    updateTimer->start();

    return RunState::Idle;
}

ManagedTask::RunState TaskScheduler::initialize(){
    disableGuiElements(runMode);

    app->saveSettings(tempSettingsPath);
    settingsSaved = true;

    // Create directory
    if (!(makeDataDirectory(dataDirectory, ui->lineEdit_relaxData->text(), app->getCurrentWorkingDirectory()))){
        return RunState::Failed;
    }

    if (!(dataDirectory.isEmpty())){
        if (QMessageBox::No == QMessageBox::question(
                    this, "Directory is not empty.", "Overwrite?", QMessageBox::Yes|QMessageBox::No)){
            return RunState::Aborted;
        }
    }

    if (!(makeDataDirectory(tempDirectory, QString("%1%2Temp%2").arg(dataDirectory.path()).arg(QDir::separator()),
                            app->getCurrentWorkingDirectory()))){
        return RunState::Failed;
    }

    tempDirectoryCreated = true;

    if (!(makeDataDirectory(tempDataDirectory, QString("%1%2Temp%2Data%2").arg(dataDirectory.path()).arg(
                                QDir::separator()), app->getCurrentWorkingDirectory()))){
        return RunState::Failed;
    }

    if (tempDataDirectory.path().endsWith("Data") and tempDataDirectory.path().contains("Temp")){
        tempDataDirectory.removeRecursively(); // Very dangerous!!!

    } else {
        QMessageBox::warning(this, "Warning!!!", "Tried to delete: " + tempDataDirectory.path());
    }

    tempConfigPath = QString("%1%2tempConfig.ini").arg(tempDirectory.path()).arg(QDir::separator());

    app->saveSettings(dataDirectory.absoluteFilePath("_config.ini"));

    QSettings settings(dataDirectory.absoluteFilePath("_config.ini"), QSettings::IniFormat);
    settings.setValue("ActiveTask", name);
    settings.sync();

    /// Copy ".dat"-file
    QFileInfo datFile(ui->lineEdit_relaxFile->text());
    QFileInfo datFileCopy(dataDirectory.absoluteFilePath(datFile.fileName()));
    if (datFileCopy.exists()){
        QFile::remove(datFileCopy.absoluteFilePath());
    }
    qDebug() << "Copy '.dat'-file to:" << datFileCopy.absoluteFilePath();
    qDebug() << QFile::copy(datFile.absoluteFilePath(), datFileCopy.absoluteFilePath());

    loadParameters();
    parametersLoaded = false;

    taskSubmitted = false;

    return RunState::Idle;
}

ManagedTask::RunState TaskScheduler::finalize()
{
    auto taskLocked = submittedTask.lock();

    if((not taskLocked.isNull()) and (not taskLocked->isFinished())){
        connectionStateChanged = connect(
            taskLocked.data(), &ManagedTask::stateChanged,
            this, &TaskScheduler::subtaskStateChanged, Qt::UniqueConnection);
        if (not taskLocked->isAborting()){
            taskLocked->abort();
        }
        return RunState::Busy;
    }

    subTaskFinished();
    return RunState::Idle;
}

void TaskScheduler::updateTasks()
{
    auto selectedTask = ui->comboBox_paTasks->currentText();
    ui->comboBox_paTasks->clear();
    for (auto &task: taskManager->getTasks()){
        if (task.isNull()){
            continue;
        }

        ui->comboBox_paTasks->addItem(task->name);
    }

    ui->comboBox_paTasks->setCurrentText(selectedTask);
}

void TaskScheduler::subtaskStateChanged()
{
    auto taskLocked = submittedTask.lock();
    if (taskLocked->isFinished()){
        subTaskFinished();
        emit alive(RunState::Idle);
    }
}

void TaskScheduler::setTimeout(int msecs){
    ManagedTask::setTimeout(msecs);
    ui->spinBox->setValue(msecs/1000);
}
void  TaskScheduler::enableGuiElements(){
    ui->groupBox->setEnabled(true);
    ui->groupBox_pulseAcquireListSettings->setEnabled(true);
    ui->groupBox_2->setEnabled(true);
}

void TaskScheduler::disableGuiElements(RunModes){
    ui->groupBox->setEnabled(false);
    ui->groupBox_pulseAcquireListSettings->setEnabled(false);
    ui->groupBox_2->setEnabled(false);
}

void TaskScheduler::prepareTask()
{
    tempDirectoryCreated = false;
}

void TaskScheduler::subTaskFinished()
{
    updateStatus();
    disconnect(connectionStateChanged);
    submittedTask.clear();

    if ((not tempSettingsPath.isEmpty()) and QFile(tempSettingsPath).exists()){
        app->loadSettings(tempSettingsPath, App::LoadOption::NoOptions);
        QFile::remove(tempSettingsPath);
    }

    if (tempDirectoryCreated){
        qDebug() << "Removing directory:" << tempDirectory.path();
        if (tempDirectory.path().endsWith("Temp")){
            tempDirectory.removeRecursively(); // Very dangerous!!!
        }
    }

    ui->lineEdit_relaxData->setText(incrementPathName(ui->lineEdit_relaxData->text()));
    updateTimer->stop();
    ui->lineEdit_progressTask->setText("");
    ui->lineEdit_taskState->setText("");
    ui->lineEdit_progressTask->setText("");

    enableGuiElements();
    return;
}

void TaskScheduler::loadParameters()
{
    // Read parameters file
    QFile mFile(ui->lineEdit_relaxFile->text());
    if(!mFile.open(QFile::ReadOnly | QFile::Text))    {
        qDebug() << "Could not open file for reading";
        return setRunState(RunState::Failed);
    }

    QString mText = QTextStream(&mFile).readAll();
    mFile.flush();
    mFile.close();

    parameterNames = mText.split("\n")[0].split("\t");
    parameterNames.removeFirst();

    counterMax = mText.split("#")[0].toInt();
    auto valueLists = mText.split("\n");

    valueLists.removeFirst();

    //Update Widget
    parameters.clear();
    for (auto i=0; i<counterMax; i++){
        parameters.insert(i, valueLists.at(i).split("\t"));
    }

    ui->tableWidget_parameters->clear();
    ui->tableWidget_parameters->setColumnCount(parameterNames.length());
    ui->tableWidget_parameters->setRowCount(parameters.size());
    ui->tableWidget_parameters->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableWidget_parameters->setHorizontalHeaderLabels(parameterNames);

    for (auto column=0; column<parameterNames.length(); column++){
        for (auto row=0; row < parameters.size(); row++){
            auto item = new QTableWidgetItem(parameters.value(row).value(column));
            item->setFlags(
                        Qt::ItemIsSelectable |
                        Qt::ItemIsEnabled
                        );

            ui->tableWidget_parameters->setItem(row, column, item);

        }
    }

    ui->tableWidget_parameters->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
}

void TaskScheduler::updateStatus()
{
    auto task = submittedTask.lock();
    if(task.isNull()){
        ui->lineEdit_progressTask->setText("");
        ui->lineEdit_taskState->setText("");
        ui->lineEdit_progressTask->setText("");
        return;
    }

    int taskCounter, taskCounterMax;
    task->getProgress(taskCounter, taskCounterMax);

    ui->progressBar_task->setValue(100*taskCounter/std::max(taskCounterMax, 1));
    ui->lineEdit_taskState->setText(QVariant::fromValue(task->getTaskState()).toString());
    ui->lineEdit_progressTask->setText(QString("%1/%2").arg(QString::number(taskCounter), QString::number(taskCounterMax)));

    ui->tableWidget_parameters->selectRow(counter);
}

void TaskScheduler::on_pushButton_openParametersFile_clicked()
{
    auto currentParent = QDir(ui->lineEdit_relaxFile->text());
    currentParent.cdUp();
    auto fileName = QFileDialog::getOpenFileName(
        this,
        "Open Parameters File",
        currentParent.absolutePath(),
        "");

    if (not QFile::exists(fileName)){
        return;
    }

    ui->lineEdit_relaxFile->setText(fileName);
    loadParameters();
}
