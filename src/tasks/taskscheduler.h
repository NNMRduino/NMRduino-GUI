#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H

#include "tasks.h"

#include <QDir>
#include <QMap>
#include <QSettings>

class QSpinBox;

namespace App{
    class MainApplication;
}

namespace Task{
    class TaskScheduler;
}

namespace Ui{
    class TaskScheduler;
}

class PulseSequence;

class Task::TaskScheduler : public Task::ManagedTask
{
   Q_OBJECT
public:
    explicit TaskScheduler(QWidget *, QString,
                           App::MainApplication* const, Task::TaskManager * const,
                           Task::TaskQueue * const);
    ~TaskScheduler() override;

    void saveSettings(QSettings &) const override;
    void loadSettings(QSettings &, const App::LoadOptions) override;

protected:
    RunState resume() override;
    RunState initialize() override;
    RunState finalize() override;

private slots:
    void updateTasks();
    void subtaskStateChanged();
    void loadParameters();
    void updateStatus();
    void on_pushButton_openParametersFile_clicked();

private:
    void setTimeout(int msecs);
    void loadNewParameters();
    void movePreviousFiles();
    void enableGuiElements() override;
    void disableGuiElements(RunModes) override;
    void prepareTask() override;
    void subTaskFinished();

private:
    QScopedPointer<Ui::TaskScheduler> ui;
    QPointer<App::MainApplication> app;
    QPointer<Task::TaskManager> taskManager;
    QPointer<Task::TaskQueue> taskQueue;

    QDir dataDirectory, tempDirectory, tempDataDirectory;

    QMap<int, QStringList> parameters;
    QStringList parameterNames;

    bool taskSubmitted, parametersLoaded, aborted;

    QString tempConfigPath;
    const QString tempSettingsPath;
    bool settingsSaved, tempDirectoryCreated;

    QMetaObject::Connection connectionStateChanged;

    QWeakPointer <Task::ManagedTask> submittedTask;
    const QScopedPointer<QTimer> updateTimer;
};

#endif // TASKSCHEDULER_H
