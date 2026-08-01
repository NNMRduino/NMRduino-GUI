#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include <QWidget>
#include <QPointer>

class QSettings;
class QElapsedTimer;

namespace App {    
    enum class LoadOption; Q_DECLARE_FLAGS(LoadOptions, LoadOption)
    class MainApplication;
}

namespace Dev {
    class DeviceManager;
}

namespace Ui {
    class TaskQueue;
    class TaskManager;
}

namespace Task{
    Q_NAMESPACE
    class TaskQueue;
    class ManagedTask;
    class TaskManager;
    enum class RunMode;
    Q_DECLARE_FLAGS(RunModes, RunMode)
}

enum class Task::RunMode{
    None            = 0x00,
    DevicesDisabled = 0x01,
    MenuDisabled    = 0x02,
    DAQDisabled     = 0x04,
}; Q_DECLARE_OPERATORS_FOR_FLAGS (Task::RunModes)

class Task::ManagedTask: public QWidget
{
    Q_OBJECT
    friend class TaskQueue;

public:
    enum class Priority{
        High        =  2,
        AboveNormal =  1,
        Normal      =  0,
        BelowNormal = -1,
        Low         = -2,
    }; Q_ENUM(Priority)

    enum class TaskType{
        None,
        SpectrumAnalyzer,
        PulseAcquire,
        TaskScheduler,
    }; Q_ENUM(TaskType)    

    enum class State{
        Ready,
        Finished,
        Failed,
        Paused,
        Aborted,
        Running,
        Finalizing,
        Initializing,
        TimedOut,
        Invalid,
    };
    Q_ENUM(State)

    enum class RunState{
        Busy,
        Idle,
        Aborted,
        Failed,
    };
    Q_ENUM(RunState)

    ManagedTask(QWidget *parent=nullptr , QString name="", TaskType taskType=TaskType::None);
    virtual ~ManagedTask() override;

    QString getTaskStatusString();
    QString getTaskState() const;

    void getProgress(int &counter) const;
    void getProgress(int &counter, int &counterMax) const;

    bool isRunning() const;
    bool isAborting() const;
    bool isFinished() const;
    bool isPaused() const;
    bool isTimedOut() const;

    virtual void setDataDirectory(QString) {}

    virtual void saveSettings(QSettings&) const;
    virtual void loadSettings(QSettings&, const App::LoadOptions) {}

    void resetTask(int newProgressCounter=0);

public slots:
    void abort();

signals:
    void stateChanged();
    void alive(RunState newRunState);
    void timedOut();
    void timeOutChanged();

protected:
    void pause();
    void run();

    void setRunState(RunState newRunState);
    void setTimeout(int msecs);
    bool isReadyToContinue();
    int getTimeout();
    int getElapsedMilliseconds() const;

    virtual void prepareTask() {}
    virtual void enableGuiElements() {}
    virtual void disableGuiElements(Task::RunModes) {}

    virtual RunState resume() {return RunState::Idle;}
    virtual RunState initialize() {return RunState::Idle;}
    virtual RunState finalize() {return RunState::Idle;}

private:
    void setState(State newState);
    void updateState();
    State getNextState();

    void estimateTaskDuration(int &secondsRemaining, int &secondsElapsed);
    void handleTimeOut();

public:
    const QString name;
    Task::RunModes runMode;
    Priority priority = Priority::Normal;
    static const QMap<Task::ManagedTask::TaskType, QString> taskTypesMap;

protected:
    int counter = 0;
    int counterMax = 1;
    RunState runState = RunState::Idle;

private:
    State state = State::Ready;    
    bool failedFlag, pausedFlag, abortedFlag, timedOutFlag;

    int oldCounter = 0;
    int counterStart = 0;
    const TaskType taskType;

    QScopedPointer<QDateTime> startTime, stopTime;

    int timeoutMsecs = 30000;
    QSharedPointer<QTimer> timeoutTimer;
};



class Task::TaskManager: public QWidget
{
    Q_OBJECT
public:
    TaskManager(QWidget* parent, App::MainApplication * const app, Dev::DeviceManager * const devManager, Task::TaskQueue * const taskQueue);
    ~TaskManager();

    void saveSettings(QSettings &settings) const;
    void loadSettings(QSettings &settings, App::LoadOptions loadOptions);

    QSharedPointer<Task::ManagedTask> createTask(const QString taskType, QString taskName);

    void removeTask(QWeakPointer<Task::ManagedTask> task);

    static QStringList getTaskTypes();
    QList<QSharedPointer<Task::ManagedTask>> getTasks() const;
    QSharedPointer<Task::ManagedTask> getTask(const QString taskName) const;

    QSharedPointer<Task::ManagedTask> getRunningTask() const;

signals:
    void tasksChanged();

private:
    QSharedPointer<Task::ManagedTask> createTask(Task::ManagedTask::TaskType taskType, const QString taskName);

private:
    QList<QSharedPointer<Task::ManagedTask>> tasks;
    QScopedPointer<Ui::TaskManager> ui;
    const QPointer<App::MainApplication> app;
    const QPointer<Dev::DeviceManager> devManager;
    const QPointer<Task::TaskQueue> taskQueue;
};



class Task::TaskQueue : public QWidget
{
    Q_OBJECT
public:
    enum class State{
        Running,
        Idle,
    };
    Q_ENUM(State)

    TaskQueue(QWidget* parent, App::MainApplication * const app);
    ~TaskQueue();

    void submitTaskToQueue(QWeakPointer<ManagedTask> task);

signals:
    void enableGuiElements();
    void disableGuiElements(Task::RunModes runMode);

private:
    void removeTaskFromQueue(QWeakPointer<ManagedTask> task);
    void updateTaskQueue();
    void setState(const State newState);

private:
    QList<QWeakPointer<Task::ManagedTask>> taskQueue;

    State state=State::Idle;

    QScopedPointer<Ui::TaskQueue> ui;
    const QPointer<App::MainApplication> app;
};

#endif // TASKMANAGER_H
