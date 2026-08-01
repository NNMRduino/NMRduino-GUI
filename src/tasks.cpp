#include "tasks.h"
#include "ui_taskqueue.h"
#include "ui_taskmanager.h"

#include <QTimer>
#include <QTime>
#include <QDebug>
#include <QSettings>
#include <QLineEdit>
#include <QInputDialog>

#include "device.h"
#include "tasks/spectrumanalyzer.h"
#include "tasks/pulseacquire.h"
#include "tasks/taskscheduler.h"
#include "mainapplication.h"

using namespace Task;

ManagedTask::ManagedTask(QWidget* parent , QString name, TaskType taskType):
    QWidget(parent),
    name(name),
    runMode(RunMode::None),
    taskType(taskType),
    startTime(new QDateTime()), stopTime(new QDateTime()),
    timeoutTimer(new QTimer(parent))
{
    timeoutTimer->setInterval(timeoutMsecs);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer.data(), &QTimer::timeout, this, &ManagedTask::handleTimeOut);
    //connect(this, &ManagedTask::alive, this, &ManagedTask::setRunState);
    connect(this, &ManagedTask::alive,
            this, [=] (RunState newRunState) {setRunState(newRunState); updateState();});
    state = State::Ready;
    runState = RunState::Idle;

    abortedFlag = false;
    failedFlag = false;
    pausedFlag = true;
    timedOutFlag = false;
    counter = 0;
}

ManagedTask::~ManagedTask(){
}

QString ManagedTask::getTaskStatusString()
{
    int secondsRemaining, secondsElapsed;
    estimateTaskDuration(secondsRemaining, secondsElapsed);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QDateTime dateNull(QDate(1, 1, 1).startOfDay());
#else
    QDateTime dateNull(QDate(1, 1, 1));
#endif
    QDateTime timeRemaining = dateNull.addSecs(secondsRemaining);
    QDateTime timeElapsed = dateNull.addSecs(secondsElapsed);
    QDateTime endTime = QDateTime::currentDateTime().addSecs(secondsRemaining);

    switch(state){
    case Task::ManagedTask::State::Ready:
    case Task::ManagedTask::State::Running:
    case Task::ManagedTask::State::Paused:    
    case Task::ManagedTask::State::Initializing:
        return QString("%1 running (%2/%3). Time remaining: %4d %5 (%6)").arg(
            name, QString::number(counter), QString::number(counterMax),
            QString::number(dateNull.daysTo(timeRemaining)), timeRemaining.toString("hh:mm:ss"),
            endTime.toString("ddd hh:mm"));
    case Task::ManagedTask::State::Finalizing:
        if (isAborting()){
            return QString("Aborting...");
        }
        return QString("Finalizing...");
    case Task::ManagedTask::State::Aborted:
        return QString("Aborted. (Last job's duration: %1d %2)").arg(
            QString::number(dateNull.daysTo(timeElapsed)), timeElapsed.toString("hh:mm:ss"));
    case Task::ManagedTask::State::Failed:
        return QString("Failed. (Last job's duration: %1d %2)").arg(
            QString::number(dateNull.daysTo(timeElapsed)), timeElapsed.toString("hh:mm:ss"));
    case Task::ManagedTask::State::Finished:
        return QString("Finished. (Last job's duration: %1d %2)").arg(
            QString::number(dateNull.daysTo(timeElapsed)), timeElapsed.toString("hh:mm:ss"));
    case Task::ManagedTask::State::Invalid:
        return "Invalid state.";
    case Task::ManagedTask::State::TimedOut:
        return QString("Timed out. (Last job's duration: %1d %2)").arg(
            QString::number(dateNull.daysTo(timeElapsed)), timeElapsed.toString("hh:mm:ss"));
    }
    return "";
}

bool ManagedTask::isRunning() const
{
    switch(state){
    case ManagedTask::State::Running:
    case ManagedTask::State::Paused:
    case ManagedTask::State::Finalizing:
    case ManagedTask::State::Initializing:
        return true;
    case ManagedTask::State::Ready:
    case ManagedTask::State::Aborted:
    case ManagedTask::State::Failed:
    case ManagedTask::State::Finished:
    case ManagedTask::State::Invalid:
    case ManagedTask::State::TimedOut:
        return false;
    }
    return false;
}

bool ManagedTask::isAborting() const
{
    if (abortedFlag){
        return true;
    }
    return false;
}

bool ManagedTask::isFinished() const
{
    switch(state){
    case ManagedTask::State::Running:
    case ManagedTask::State::Paused:
    case ManagedTask::State::Finalizing:
    case ManagedTask::State::Ready:
    case ManagedTask::State::Initializing:
        return false;
    case ManagedTask::State::Aborted:
    case ManagedTask::State::Failed:
    case ManagedTask::State::Finished:
    case ManagedTask::State::Invalid:
    case ManagedTask::State::TimedOut:
        return true;
    }
    qCritical();
    return false;
}

bool ManagedTask::isPaused() const
{
    if (state == ManagedTask::State::Paused){
        return true;
    }
    return false;

}

bool ManagedTask::isTimedOut() const
{
    if (timedOutFlag){
        return true;
    }
    return false;
}

void ManagedTask::setState(State newState)
{
    State oldState = state;
    state = newState;
    qDebug() << oldState << "->" << newState << "(" << runState << ")";

    if (oldState == state){
        // Steady state reached.
        emit stateChanged();
        return;
    }

    switch(newState){
    case State::Ready:
        switch(oldState){
        case State::Aborted:
        case State::Finished:
        case State::Failed:
        case State::TimedOut:
            break;
        case State::Running:
        case State::Paused:
        case State::Initializing:
        case State::Finalizing:
        case State::Ready:
        case State::Invalid:
           qCritical() << "Bad state transitions" << oldState << "->" << newState;
        }
        break;

    case State::Initializing:
        switch(oldState){
        case State::Ready:
            counterStart = counter;
            *startTime = QDateTime::currentDateTime();
            setRunState(initialize());
            break;
        case State::Running:
        case State::Finalizing:
        case State::Paused:
        case State::Finished:
        case State::TimedOut:
        case State::Aborted:
        case State::Failed:
        case State::Initializing:
        case State::Invalid:
            qCritical() << "Bad state transitions" << oldState << "->" << newState;
            break;
        }
        break;

    case State::Paused:
        switch(oldState){
        case State::Running:
        case State::Initializing:
            // Fine
            break;
        case State::Ready:
        case State::Aborted:
        case State::Finished:
        case State::Finalizing:
        case State::TimedOut:
        case State::Paused:
        case State::Failed:
        case State::Invalid:
            qCritical() << "Bad state transitions" << oldState << "->" << newState;
        }
        break;

    case State::Running:
        switch(oldState){
        case State::Initializing:
        case State::Paused:
            setRunState(resume());
            break;
        case State::Running:
        case State::Failed:
        case State::Finalizing:
        case State::Finished:
        case State::Aborted:
        case State::Ready:
        case State::Invalid:
        case State::TimedOut:
            qCritical() << "Bad state transitions" << oldState << "->" << newState;
        }
        break;

    case State::Finished:      
    case State::Aborted:
    case State::Failed:
    case State::TimedOut:
        switch(oldState){
        case State::Finalizing:
            *stopTime = QDateTime::currentDateTime();
            pausedFlag = true;
            break;
        case State::Ready:
            pausedFlag = true;
            break;
        case State::Paused:
        case State::Finished:
        case State::Aborted:
        case State::Initializing:
        case State::Invalid:
        case State::Running:
        case State::Failed:
        case State::TimedOut:
            qCritical() << "Bad state transitions" << oldState << "->" << newState;
        }
        break;

    case State::Finalizing:
        switch(oldState){
        case State::Running:
        case State::Paused:
        case State::Initializing:
            setRunState(finalize());
            break;
        case State::Finalizing:
        case State::Finished:
        case State::TimedOut:
        case State::Failed:
        case State::Aborted:
        case State::Invalid:
        case State::Ready:
            qCritical() << "Bad state transitions" << oldState << "->" << newState;
        }
        break;

    case State::Invalid:
        qCritical() << "Bad state transitions" << oldState << "->" << newState;
        break;
    }

    updateState();
}

void ManagedTask::estimateTaskDuration(int &secondsRemaining, int &secondsTotal) {
    auto milliSecondsElapsed = startTime->msecsTo(QDateTime::currentDateTime());
    if (oldCounter != counter){
        oldCounter = counter;

        if ((counter > counterStart) and (counterStart < counterMax)){
            auto counterDone = counter - counterStart;
            auto counterTotal = counterMax - counterStart;
            double ratioDone = static_cast<double>(counterTotal) / counterDone;
            qint64 totalMSecs = milliSecondsElapsed * ratioDone;
            *stopTime = startTime->addMSecs(totalMSecs);
        }
    }

    secondsRemaining = QDateTime::currentDateTime().secsTo(*stopTime);
    secondsTotal = startTime->secsTo(*stopTime);
}

void ManagedTask::getProgress(int &progressCounter) const
{
    progressCounter = counter;
}

void ManagedTask::getProgress(int &progressCounter, int &progressCounterrMax) const
{
    progressCounter = counter;
    progressCounterrMax = counterMax;
}

QString ManagedTask::getTaskState() const{
    return QVariant::fromValue(state).toString();
}

void ManagedTask::run(){
    // runFlag = true;
    pausedFlag = false;
    updateState();
}



void ManagedTask::updateState()
{
    setState(getNextState());
}

ManagedTask::State ManagedTask::getNextState()
{
    switch(state){
    case State::Aborted:
        if (failedFlag || abortedFlag || pausedFlag || timedOutFlag){
            return State::Aborted;
        }
        return State::Ready;

    case State::Failed:
        if (failedFlag || abortedFlag || pausedFlag || timedOutFlag){
            return State::Failed;;
        }
        return State::Ready;

    case State::Finished:
        if (failedFlag || abortedFlag || pausedFlag || timedOutFlag){
            return State::Finished;
        }
        return State::Ready;

    case State::TimedOut:
        if (failedFlag || abortedFlag || pausedFlag || timedOutFlag){
            return State::TimedOut;
        }
        return State::Ready;

    case State::Ready:
        if (timedOutFlag) {
            return State::TimedOut;
        } else if (failedFlag){
            return State::Failed;
        } else if (abortedFlag){
            return State::Aborted;
        } else if (pausedFlag) {
            return State::Ready;
        }
        return State::Initializing;

    case State::Initializing:
        if (failedFlag or abortedFlag or timedOutFlag) {
            return State::Finalizing;
        } else if (runState == RunState::Busy){
            return State::Initializing;
        } else if (pausedFlag) {
            return State::Paused;
        }
        return State::Running;

    case State::Paused:
        if (failedFlag or abortedFlag or timedOutFlag){
            return State::Finalizing;
        } else if (pausedFlag) {
            return State::Paused;
        }
        return State::Running;

    case State::Running:
        if (failedFlag or abortedFlag or timedOutFlag){
            return State::Finalizing;
        } else if (runState == RunState::Busy){
            return State::Running;
        } else if (pausedFlag) {
            return State::Paused;
        }
        return State::Finalizing;

    case State::Finalizing:
        if (timedOutFlag){
            return State::TimedOut;
        } else if (runState == RunState::Busy) {
            return State::Finalizing;
        } else if (failedFlag) {
            return State::Failed;
        } else if (abortedFlag) {
            return State::Aborted;
        }

        return State::Finished;

    case State::Invalid:
        return State::Invalid;
    }

    qCritical() << "Error in state machine.";
    return State::Invalid;
}

void ManagedTask::setRunState(RunState newRunState)
{
    qDebug() << runState << "->" << newRunState;

    runState = newRunState;

    switch(runState){
    case RunState::Idle:
        timeoutTimer->stop();
        break;
    case RunState::Busy:
        timeoutTimer->start();
        break;
    case RunState::Aborted:
        timeoutTimer->stop();
        abortedFlag = true;
        break;
    case RunState::Failed:
        timeoutTimer->stop();
        failedFlag = true;
        break;
    }

    //if(updateStateAfter){
    //    updateState();
    //}
}

bool ManagedTask::isReadyToContinue()
{
    if (abortedFlag || pausedFlag || failedFlag){
        return false;
    }

    return true;
}

int ManagedTask::getElapsedMilliseconds() const
{
    if (timeoutTimer->isActive()){
        return timeoutTimer->interval() - timeoutTimer->remainingTime();
    } else {
        return -1;
    }
}

void ManagedTask::setTimeout(int msecs)
{
    int oldTimeoutMsecs = timeoutMsecs;
    timeoutMsecs = msecs;
    timeoutTimer->setInterval(timeoutMsecs);
    if(oldTimeoutMsecs != timeoutMsecs){
        emit timeOutChanged();
    }
}

int ManagedTask::getTimeout()
{
    return timeoutMsecs;
}

void ManagedTask::saveSettings(QSettings &settings) const
{
    settings.setValue("TaskType", taskTypesMap.value(taskType));
    settings.setValue("TaskName", name);
}

void ManagedTask::resetTask(int newProgressCounter){
    if (isRunning()){
        qCritical() << "Task is still running!";
        return;
    }

    counter = newProgressCounter;

    timedOutFlag = false;
    failedFlag = false;
    abortedFlag = false;
    pausedFlag = true;
    runState = RunState::Idle;
    state = State::Ready;

    prepareTask();

    updateState();
}

void ManagedTask::abort() {
    abortedFlag = true;
    updateState();
}

void ManagedTask::handleTimeOut() {
    qCritical() << "Timed out.";
    timedOutFlag = true;
    abortedFlag = true;
    emit timedOut();
    updateState();
}

void ManagedTask::pause() {
    pausedFlag = true;
    updateState();
}

const QMap<ManagedTask::TaskType, QString> ManagedTask::taskTypesMap = QMap<ManagedTask::TaskType, QString>(
{
           {ManagedTask::TaskType::SpectrumAnalyzer,    "Spectrum Analyzer"},
           {ManagedTask::TaskType::PulseAcquire,        "Pulse and Acquire"},
           {ManagedTask::TaskType::TaskScheduler,       "Task Scheduler"},
    });






TaskQueue::TaskQueue(QWidget *parent, App::MainApplication * const app):
    QWidget(parent),
    ui(new Ui::TaskQueue),
    app(app)
{
    ui->setupUi(this);
}

TaskQueue::~TaskQueue(){

}

void TaskQueue::submitTaskToQueue(QWeakPointer<ManagedTask> task){
    auto taskLocked = task.lock();
    if (taskLocked.isNull()){
        return;
    }

    // Remove task, if it is already in queue
    removeTaskFromQueue(task);

    // Find insert position
    int i = 0;
    for (i=0; i<taskQueue.length(); i++){
        auto queuedTaskLocked = taskQueue[i].lock();
        if (queuedTaskLocked.isNull()){
            continue;
        }

        if (static_cast<int>(taskLocked->priority) > static_cast<int>(queuedTaskLocked->priority)){
           break;
        }
    }

    // If the new task has lower/equal priority, it will be inserted at the end of the list
    taskQueue.insert(i, taskLocked);
    taskLocked->disableGuiElements(taskLocked->runMode);
    connect(taskLocked.data(), &ManagedTask::stateChanged, this, &TaskQueue::updateTaskQueue);
    updateTaskQueue();
}

void TaskQueue::removeTaskFromQueue(QWeakPointer<ManagedTask> task){
    auto taskLocked = task.lock();

    if (taskLocked.isNull())
    {
        taskQueue.removeAll(taskLocked);
        return;
    }

    if (taskLocked->isRunning()){
         taskLocked->abort();
    } else {
         disconnect(taskLocked.data(), &ManagedTask::stateChanged,
                    this,              &TaskQueue::updateTaskQueue);
         taskQueue.removeAll(taskLocked);
         taskLocked->enableGuiElements();
    }
}

void TaskQueue::updateTaskQueue(){
    // update widget
    ui->listWidget->clear();
    for(auto &task: qAsConst(taskQueue)){
        auto taskLocked = task.lock();
        QString taskState =  QVariant::fromValue(taskLocked->getTaskState()).toString();
        ui->listWidget->addItem(taskLocked->name + " (" + taskState + ")");
    }

    if (taskQueue.isEmpty()){
        setState(State::Idle);
        return;
    } else {
        setState(State::Running);
    }


    auto firstTaskLocked = taskQueue.first().lock();
    if (firstTaskLocked.isNull()){
        removeTaskFromQueue(firstTaskLocked);
        updateTaskQueue();
        return;
    }

    if (firstTaskLocked->isFinished()){
        removeTaskFromQueue(firstTaskLocked);
        updateTaskQueue();
        return;
    } else if (firstTaskLocked->isRunning() and not firstTaskLocked->isPaused()){
        return;
    }

    for(auto &task: qAsConst(taskQueue)){
        auto taskLocked = task.lock();
        if (taskLocked.isNull()){
            continue;
        }

        if(taskLocked == firstTaskLocked){
            continue;
        }

        if (taskLocked->isRunning() and (not taskLocked->isPaused())){
            taskLocked->pause();
            return;
        }
    }

    emit disableGuiElements(firstTaskLocked->runMode);
    firstTaskLocked->run();
}

void TaskQueue::setState(const TaskQueue::State newState){
    if (newState == state){
         return;
    }

    qDebug()  << newState;
    state = newState;

    switch(state){
    case State::Running:
        emit disableGuiElements(RunMode::DevicesDisabled | RunMode::MenuDisabled);
        return;

    case State::Idle:
        emit enableGuiElements();
        return;
    }
}





TaskManager::TaskManager(QWidget *parent, App::MainApplication * const app, Dev::DeviceManager * const devManager, TaskQueue * const taskQueue):
    QWidget(parent),
    ui(new Ui::TaskManager),
    app(app),
    devManager(devManager),
    taskQueue(taskQueue)
{
    ui->setupUi(this);
}

TaskManager::~TaskManager()
{

}

void TaskManager::saveSettings(QSettings &settings) const
{

    settings.setValue("TaskManager/ActiveTaskIndex",   ui->tabWidget->currentIndex());

    QStringList listTaskNames;

    for(auto &task: qAsConst(tasks)){
        listTaskNames.append(task->name);
    }
    settings.setValue("TaskManager/TaskNames", listTaskNames);

    for(auto &task: qAsConst(tasks)){
        settings.beginGroup(task->name);
        task->saveSettings(settings);
        settings.endGroup();
    }
}

void TaskManager::loadSettings(QSettings &settings, App::LoadOptions loadOptions)
{
    if (loadOptions & App::LoadOption::ReopenTasks){
        for(auto &task: qAsConst(tasks)){
            removeTask(task);
        }

        for(auto &taskName: settings.value("TaskManager/TaskNames").toStringList()){
            settings.beginGroup(taskName);
            auto taskType = ManagedTask::taskTypesMap.key(settings.value("TaskType").toString());
            settings.endGroup();
            createTask(taskType, taskName);
        }
    }

    for(auto task: qAsConst(tasks)){
        if(task.isNull()){
            continue;
        }
        settings.beginGroup(task->name);
        task->loadSettings(settings, loadOptions);
        settings.endGroup();
    }

    // Load other settings
    if (loadOptions & App::LoadOption::LoadActiveIndices){
        ui->tabWidget->setCurrentIndex(settings.value("TaskManager/ActiveTaskIndex"  ).toInt());
    }
}

QSharedPointer<ManagedTask> TaskManager::createTask(ManagedTask::TaskType taskType, const QString taskName)
{
    QSharedPointer<ManagedTask> task;
    switch(taskType){
    case ManagedTask::TaskType::None:
        break;
    case ManagedTask::TaskType::SpectrumAnalyzer:
        task = QSharedPointer<SpectrumAnalyzer>::create(ui->tabWidget, taskName, app, devManager);
        break;
    case ManagedTask::TaskType::PulseAcquire:
        task = QSharedPointer<PulseAcquire>::create(ui->tabWidget, taskName, app, devManager);
        break;
    case ManagedTask::TaskType::TaskScheduler:
        task = QSharedPointer<TaskScheduler>::create(ui->tabWidget, taskName, app, this, taskQueue);
        break;
    }

    if(task.isNull()){
        return QSharedPointer<ManagedTask>();
    }

    tasks.append(task);
    ui->tabWidget->addTab(task.data(), task->name);
    emit tasksChanged();
    return task;
}

QSharedPointer<ManagedTask> TaskManager::createTask(const QString taskType, QString taskName)
{
    if (taskName.isEmpty())
    {
        bool ok;
        taskName = QInputDialog::getText(this, "Adding a new measurement", "Please choose a name:", QLineEdit::Normal,
                                         "", &ok, Qt::MSWindowsFixedSizeDialogHint);
        if(not ok){
            return QSharedPointer<ManagedTask>();
        }
    }

    auto task = createTask(ManagedTask::taskTypesMap.key(taskType), taskName);

    if (task.isNull()){
        return task;
    }

    QSettings settings(app->getSettingsFilePath(), QSettings::IniFormat);

    settings.beginGroup(taskName);
    task->loadSettings(settings, App::LoadOption::NoOptions);
    settings.endGroup();

    return task;
}

void TaskManager::removeTask(QWeakPointer<ManagedTask> task)
{
    if (not task.lock()->isRunning()){
        tasks.removeAll(task.lock());
        task.clear();
        emit tasksChanged();
    } else {
        qCritical() << "Error: Task is still running.";
    }
}

QStringList TaskManager::getTaskTypes()
{
    return ManagedTask::taskTypesMap.values();
}

QList<QSharedPointer<ManagedTask> > TaskManager::getTasks() const
{
    QList<QSharedPointer<Task::ManagedTask>> tasksList;

    for(auto &task: qAsConst(tasks)){
        if (not task.isNull()){
          tasksList.append(task);
        }
    }

    return tasksList;
}

QSharedPointer<ManagedTask> TaskManager::getTask(const QString taskName) const
{
    for(auto &task: tasks){
        if (task->name == taskName){
            return task;
        }
    }
    return QSharedPointer<ManagedTask>();
}

QSharedPointer<ManagedTask> TaskManager::getRunningTask() const
{
    for(auto &task: qAsConst(tasks)){
        if(task != ui->tabWidget->currentWidget()){
            continue;
        }
        return task;
    }
    return QSharedPointer<ManagedTask>();
}
