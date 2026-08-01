#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QInputDialog>
#include <QTimer>
#include <QDebug>
#include <QAction>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QDate>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QSplitter>
#include <QColorDialog>

#include "mainapplication.h"
#include "device.h"
#include "tasks.h"
#include "spoiler.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(App::MainApplication* const app):
    ui(new Ui::MainWindow),
    taskQueue(new Task::TaskQueue(this, app)),
    devManager(new Dev::DeviceManager(this, app)),
    app(app),
    updateTimer(new QTimer)
{
    ui->setupUi(this);

    #ifdef Q_OS_WIN
    ui->lineEdit_cwd->setText(QDir::cleanPath(QDir::homePath()                        + QDir::separator() +
    #else
    ui->lineEdit_cwd->setText(QDir::cleanPath(QString("/home/pi")                     + QDir::separator() +
    #endif
                                              "Desktop"                               + QDir::separator() +
                                              "data"                                  + QDir::separator() +
                                              QDate::currentDate().toString("yyMMdd") + QDir::separator()));

    ui->menuBar->setVisible(true);

    updateTimer->setInterval(50);
    connect(updateTimer.data(), &QTimer::timeout, this, &MainWindow::updateStatus);

    taskManager = QPointer<Task::TaskManager>(new Task::TaskManager(this, app, devManager, taskQueue));

    ui->widget_taskManager->layout()->addWidget(taskManager);

    auto splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(devManager);
    splitter->addWidget(taskQueue);
    splitter->setSizes({1, 0});

    auto spoiler = new Spoiler();
    spoiler->addWidget(splitter);

    dynamic_cast<QGridLayout*>(ui->centralwidget->layout())->addWidget(spoiler, 0, 1, 2, 1);

    auto const devTypes = devManager->getDevTypes();
    for(auto &deviceType: devTypes){
        auto qAction = ui->menuAdd->addAction(deviceType);
        connect(qAction, &QAction::triggered, devManager.data(), [=](){devManager->createDevice(deviceType, "");});
    }

    auto const taskTypes = taskManager->getTaskTypes();
    for(auto &taskType: taskTypes){
        auto qAction = ui->menuAddMeasurement->addAction(taskType);
        connect(qAction, &QAction::triggered, taskManager.data(), [=](){taskManager->createTask(taskType, "");});
    }

    connect(ui->pushButton, &QPushButton::clicked, this, [=] () {runTask(taskManager->getRunningTask());});
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::abortTask);

    connect(taskQueue.data(), &Task::TaskQueue::enableGuiElements, this, &MainWindow::enableGuiElements);    
    connect(taskQueue.data(), &Task::TaskQueue::disableGuiElements,this, &MainWindow::disableGuiElements);

    connect(taskQueue.data(), &Task::TaskQueue::disableGuiElements,
            devManager.data(), &Dev::DeviceManager::disableGuiElements);
    connect(taskQueue.data(), &Task::TaskQueue::enableGuiElements,
            devManager.data(), &Dev::DeviceManager::enableGuiElements);

    connect(devManager.data(), &Dev::DeviceManager::devicesChanged, this, &MainWindow::updateDevices);
    connect(taskManager.data(), &Task::TaskManager::tasksChanged, this, &MainWindow::updateTasks);

    setWindowTitle(QString("NMRduino (Version %1)").arg(app->version));
}

MainWindow::~MainWindow() {}

void MainWindow::saveSettings(QSettings &settings) const
{
    saveActiveTaskStatus(settings);

    devManager->saveSettings(settings);
    taskManager->saveSettings(settings);
}


void MainWindow::loadSettings(QSettings &settings, App::LoadOptions loadOptions)
{
    // Load devices
    devManager->loadSettings(settings, loadOptions);

    // Load tasks
    taskManager->loadSettings(settings, loadOptions);

    settings.beginGroup("MainWindow");
    if (loadOptions & App::LoadOption::ResumeTask){
        auto taskName = settings.value("ActiveTask").toString();
        auto progressCounter = settings.value("ActiveTaskProgress").toInt();

        QTimer::singleShot(0, this, [=] () {resumeTask(taskName, progressCounter);});
    }

    settings.endGroup();
}

const QString MainWindow::getCurrentWorkingDirectory() const{
    return ui->lineEdit_cwd->text();
}

void MainWindow::updateStatus(){
    static int oldCounter = 0;

    auto currentTaskLocked = activeTask.lock();
    if (currentTaskLocked.isNull()){
        updateTimer->stop();
        ui->lineEdit_runningTask->setText("");
        ui->pushButton->setEnabled(true);
        ui->pushButton_2->setEnabled(false);
        return;
    }

    int counter, counterMax;
    currentTaskLocked->getProgress(counter, counterMax);

    if (oldCounter != counter){
        oldCounter = counter;
        QSettings qSettings(app->getSettingsFilePath(), QSettings::IniFormat);
        qSettings.beginGroup("MainWindow");
        saveActiveTaskStatus(qSettings);
        qSettings.endGroup();
        qSettings.sync();
    }

    if( currentTaskLocked->isRunning()){
        updateTimer->start();        
        ui->pushButton->setEnabled(false);
        ui->pushButton_2->setEnabled(true);
#ifdef Q_OS_WIN
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
#endif
    } else {
        updateTimer->stop();
        ui->pushButton->setEnabled(true);
        ui->pushButton_2->setEnabled(false);
        app->saveSettings();
#ifdef Q_OS_WIN
        SetThreadExecutionState(ES_CONTINUOUS);
#endif
    }

    ui->lineEdit_runningTask->setText(currentTaskLocked->name);
    ui->statusbar->showMessage(currentTaskLocked->getTaskStatusString());
    ui->progressBar->setValue(100*counter/std::max(counterMax, 1));
}

void MainWindow::on_actionChange_Color_triggered()
{
    auto color = QColorDialog::getColor();

    if (not color.isValid()){
        return;
    }
    app->changeStyleSheetParameter("mainColor", color.name().replace("#", ""));
}

void MainWindow::on_actionChange_Secondary_Color_triggered()
{
    auto color = QColorDialog::getColor();

    if (not color.isValid()){
        return;
    }
    app->changeStyleSheetParameter("secondaryColor", color.name().replace("#", ""));
}

void MainWindow::on_actionShow_path_to_config_ini_triggered()
{
    QMessageBox::information(
        this,
        "Path to \"_config.ini\" file",
        app->getSettingsFilePath()
        );
}

void MainWindow::updateDevices()
{
    ui->menuRemove->clear();
    auto devices = devManager->getDevices();
    for(auto& device: qAsConst(devices)){
        auto qAction(ui->menuRemove->addAction(device->deviceName));
        auto deviceWeakPointer = device.toWeakRef();
        connect(qAction, &QAction::triggered, this, [=] () {devManager->removeDevice(deviceWeakPointer);});
        connect(deviceWeakPointer.lock().data(), &Dev::GeneralDevice::destroyed, qAction, &QAction::deleteLater);
    }
}

void MainWindow::updateTasks()
{
    ui->menuRemove_2->clear();
    auto tasks = taskManager->getTasks();
    for(auto& task: qAsConst(tasks)){
        auto qAction(ui->menuRemove_2->addAction(task->name));
        auto taskWeakPointer = task.toWeakRef();
        connect(qAction, &QAction::triggered, this, [=] () {taskManager->removeTask(taskWeakPointer);});
        connect(taskWeakPointer.lock().data(), &Task::ManagedTask::destroyed, qAction, &QAction::deleteLater);
    }
}

void MainWindow::runTask(QSharedPointer<Task::ManagedTask> task, int progressCounter){
    auto activeTaskLocked = activeTask.lock();

    if (!activeTaskLocked.isNull()){
        if(not activeTaskLocked->isRunning()){
            disconnect(activeTaskLocked.data(), &Task::ManagedTask::stateChanged,
                       this, &MainWindow::updateStatus);
            activeTaskLocked.clear();
        } else {
            QMessageBox::critical(this, "Error", "Old task is still running.");
            return;
        }
    }

    activeTask = task;

    task->resetTask(progressCounter);

    connect(task.data(), &Task::ManagedTask::stateChanged,
            this, &MainWindow::updateStatus);

    app->saveSettings();
    taskQueue->submitTaskToQueue(task);
}

void MainWindow::abortTask(){
    auto currentTaskLocked = activeTask.lock();

    if (currentTaskLocked.isNull()){
        updateStatus();
        return;
    }

    currentTaskLocked->abort();
}

void MainWindow::resumeTask(QString taskName, int progressCounter)
{
    auto tasksList = taskManager->getTasks();
    for(auto &task: qAsConst(tasksList)){
        if(task->name != taskName){
            continue;
        }

        runTask(task, progressCounter);
        break;
    }
}

void MainWindow::saveActiveTaskStatus(QSettings &settings) const
{
    auto currentTaskLocked = activeTask.lock();

    if (currentTaskLocked.isNull()){
        settings.setValue("ActiveTask", "");
        settings.setValue("ActiveTaskProgress", 0);
        return;
    }

    int counter;
    QString name;

    if (currentTaskLocked->isRunning() and (not currentTaskLocked->isAborting())){
        name = currentTaskLocked->name;
        currentTaskLocked->getProgress(counter);
    } else {
        name = "";
        counter = 0;
    }

    settings.setValue("ActiveTask", name);
    settings.setValue("ActiveTaskProgress", counter);
}

void MainWindow::disableGuiElements(Task::RunModes runMode){
    if (runMode & Task::RunMode::MenuDisabled){
        ui->menuBar->setEnabled(false);
    } else {
        ui->menuBar->setEnabled(true);
    }
    ui->pushButton->setEnabled(false);
    ui->pushButton_2->setEnabled(true);
}

void MainWindow::enableGuiElements(){
    ui->menuBar->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->pushButton_2->setEnabled(false);
}
