#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QSettings;

namespace App {
    enum class LoadOption; Q_DECLARE_FLAGS(LoadOptions, LoadOption)
    class MainApplication;
}

namespace Dev {
    class GeneralDevice;
    class MainZulf;
    class DeviceManager;
}

namespace Task {
    class ManagedTask;
    class TaskQueue;
    class TaskManager;
    enum class RunMode; Q_DECLARE_FLAGS(RunModes, RunMode)
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(App::MainApplication* const app);
    ~MainWindow();

    void saveSettings(QSettings &settings) const;
    void loadSettings(QSettings &settings, App::LoadOptions loadOptions);

    const QString getCurrentWorkingDirectory() const;

    void abortTask();

private slots:
    void updateStatus();
    void on_actionChange_Color_triggered();
    void on_actionChange_Secondary_Color_triggered();
    void on_actionShow_path_to_config_ini_triggered();

private:
    void updateDevices();
    void updateTasks();

    void runTask(QSharedPointer<Task::ManagedTask> task, int progressCounter=0);
    void resumeTask(QString taskName, int progressCounter);
    void saveActiveTaskStatus(QSettings &settings) const;

    void disableGuiElements(Task::RunModes runMode);
    void enableGuiElements();    

private:
    QScopedPointer<Ui::MainWindow> ui;

    const QPointer<Task::TaskQueue> taskQueue;
    const QPointer<Dev::DeviceManager> devManager;
    const QPointer<App::MainApplication> app;
    QPointer<Task::TaskManager> taskManager;

    QWeakPointer<Task::ManagedTask> activeTask;

    const QScopedPointer<QTimer> updateTimer;
};
#endif // MAINWINDOW_H
