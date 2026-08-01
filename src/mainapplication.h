#ifndef MAINAPPLICATION_H
#define MAINAPPLICATION_H

#include <QObject>
#include <QSharedPointer>
#include <QMap>
#include <QSettings>
#include <QDir>

class QSettings;

namespace App{
    Q_NAMESPACE
    class MainApplication;
    enum class LoadOption;
    Q_DECLARE_FLAGS(LoadOptions, LoadOption)
    typedef QMap<std::string, QColor> ColorPalette;
}

namespace Task{
    class TaskQueue;
    class ManagedTask;
}

enum class App::LoadOption {
    NoOptions           = 0x00,
    LoadActiveIndices   = 0x01,
    SetLarmorFrequency  = 0x02,
    LoadPulseSequence   = 0x04,
    ReconnectDevices    = 0x08,
    ReopenTasks         = 0x10,
    //SetTestSignal       = 0x20,
    ResumeTask          = 0x40,
    SkipWarnings        = 0x80,
    LoadPlotSettings    = 0x100,
};
Q_DECLARE_OPERATORS_FOR_FLAGS (App::LoadOptions)

class MainWindow;

class App::MainApplication: public QObject{
    Q_OBJECT
public:    
    explicit MainApplication(QObject *parent = nullptr);
    ~MainApplication();

    void loadSettings(const QString filePath, const App::LoadOptions options=App::LoadOption::NoOptions);
    void loadSettings(const App::LoadOptions options=App::LoadOption::NoOptions);
    void loadSettings(QSettings &settings, App::LoadOptions loadOptions=App::LoadOption::NoOptions);

    void saveSettings(QString filePath = "") const;
    void saveSettings(QSettings &settings) const;

    void changeStyleSheetParameter(QString name, QString value, bool load=true);

    const QString getTempPath() const;
    const QString getCurrentWorkingDirectory() const;
    const QString getSettingsFilePath() const;

    static QDir getConfigLocation();

private:
    void loadStyleSheet();
    void loadDefaultPalette();
    void updateSVGFiles(QString &stylesheet);

public:
    static const QString version;

private:    
    QScopedPointer<MainWindow> mainWindow;
    QString settingsFilePath;
    const QSettings defaultConfig;
    ColorPalette colorPalette;
};
#endif // MAINAPPLICATION_H
