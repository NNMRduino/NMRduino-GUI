#include <QDebug>
#include <QApplication>
#include <QDateTime>
#include <QSettings>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

#include "mainwindow.h"
#include "mainapplication.h"
#include "device.h"
#include "tasks.h"

using namespace App;

const QString MainApplication::version(APP_VERSION);

MainApplication::MainApplication(QObject *parent) :
    QObject(parent),
    mainWindow(new MainWindow(this)),
    defaultConfig(":/default_config.ini", QSettings::IniFormat)
{
    qInfo() << "Program started.";

    settingsFilePath = MainApplication::getConfigLocation().filePath("_config.ini");

    if(not QFile(settingsFilePath).exists()){
        QFile(":/default_config.ini").copy(settingsFilePath);
        QSettings(settingsFilePath).remove("ColorsNoUser");
    }

    LoadOptions loadOptions = LoadOption::ReconnectDevices |
                              LoadOption::LoadActiveIndices |
                              LoadOption::ReopenTasks |
                              LoadOption::LoadPlotSettings |
                              LoadOption::LoadPulseSequence;

    if(qApp->arguments().indexOf("-resumeTask") >= 0){
        qDebug() << "Resume tasks!";
        loadOptions |= LoadOption::ResumeTask | LoadOption::SkipWarnings;
    }

    loadDefaultPalette();

    loadSettings(loadOptions);

    mainWindow->show();
    mainWindow->resize(800,600);
    //mainWindow->showMaximized();

    // qApp->setStyleSheet("QWidget {border: 1px solid red}");
}

MainApplication::~MainApplication(){
    mainWindow->abortTask();
    saveSettings();
    qInfo() << "Program closed.";
}


void MainApplication::loadSettings(const QString filePath, const App::LoadOptions options){
    if (not QFile::exists(filePath)){
        qWarning() << "Settings file not found:" << filePath;
        //return;
    }

    QSettings settings(filePath, QSettings::IniFormat);

    loadSettings(settings, options);
}

void MainApplication::loadSettings(const App::LoadOptions options){
    loadSettings(settingsFilePath, options);
}

void MainApplication::loadSettings(QSettings &settings, App::LoadOptions options){
    settings.beginGroup("General");
    settings.endGroup();

    settings.beginGroup("Colors");
    changeStyleSheetParameter("mainColor", settings.value("mainColor").toString(), false);
    changeStyleSheetParameter("secondaryColor", settings.value("secondaryColor").toString(), false);
    settings.endGroup();

    loadStyleSheet();

    mainWindow->loadSettings(settings, options);

    qDebug() << "Settings have been loaded: " << settings.fileName();
}



void MainApplication::saveSettings(QString filePath) const{
    if (filePath.isEmpty())    {
        filePath = settingsFilePath;
    }

    QSettings settings(filePath, QSettings::IniFormat);

    saveSettings(settings);

    QFile settingsFile(filePath);
    settingsFile.setPermissions(settingsFile.permissions() | QFileDevice::WriteGroup);
}

void MainApplication::saveSettings(QSettings &settings) const{
    settings.setValue("Version", version);
    settings.setValue("DateTime", QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"));

    settings.beginGroup("Colors");
    settings.setValue("mainColor", colorPalette.value("mainColor").name().replace("#", ""));
    settings.setValue("secondaryColor", colorPalette.value("secondaryColor").name().replace("#", ""));
    settings.endGroup();

    mainWindow->saveSettings(settings);

    qDebug() << "Settings have been saved: " << settings.fileName();
}

void MainApplication::changeStyleSheetParameter(QString name, QString value, bool load)
{
    if(not QColor(QString("#%1").arg(value)).isValid()){
        return;
    }

    QColor color(QString("#%1").arg(value));

    if (colorPalette.value(name.toStdString()) == color){
        return;
    }

    colorPalette.insert(name.toStdString(), color);
    if (load){
        loadStyleSheet();
    }
}

const QString MainApplication::getTempPath() const
{
    static int tempPathCounter(0);
    return MainApplication::getConfigLocation().filePath("_temp_config_%1.ini").arg(tempPathCounter++);
}

const QString MainApplication::getCurrentWorkingDirectory() const{
    return mainWindow->getCurrentWorkingDirectory();
}

const QString MainApplication::getSettingsFilePath() const
{
    return settingsFilePath;
}

QDir MainApplication::getConfigLocation()
{
    QDir appDir(QApplication::applicationDirPath());
    QFile settingsFile(appDir.filePath("_config.ini"));

    if(settingsFile.exists()){
        return appDir;
    }

    QString homeLocationPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    QDir homeLocationAppDir(homeLocationPath + "/.NMRduino");

    if(homeLocationAppDir.exists()){
        return homeLocationAppDir;
    }

    if (homeLocationAppDir.mkpath(".")){
        return homeLocationAppDir;
    }

    return appDir;
}

void MainApplication::loadStyleSheet()
{
    QFile file(":/dark/stylesheet.qss");
    file.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(file.readAll());

    if(colorPalette.value("mainColor") != QColor("#3daee9")){
        updateSVGFiles(styleSheet);
    }

    const auto keys = colorPalette.keys();
    for(const auto &key: keys){
        styleSheet.replace(QString("@%1@").arg(QString::fromStdString(key)), colorPalette.value(key).name());
    }

    mainWindow->setStyleSheet(styleSheet);
}

void MainApplication::loadDefaultPalette()
{
    const auto keys = defaultConfig.allKeys();
    for(const auto &key: keys){
        if ( not (key.startsWith("Colors/") or key.startsWith("ColorsNoUser/"))){
            continue;
        }
        QColor color("#" + defaultConfig.value(key).toString());

        // "Colors/key" ->  "key"
        QString keyStripped = key;
        keyStripped.replace("Colors/", "").replace("ColorsNoUser/", "");
        colorPalette.insert(keyStripped.toStdString(), color);
    }
}

void MainApplication::updateSVGFiles(QString &stylesheet)
{
    QDir tempDir = QFileInfo(settingsFilePath).dir();
    if (not tempDir.exists("temp")){
        tempDir.mkdir("temp");
    }
    tempDir.cd("temp");

    const QStringList fileList({
        ":/dark/checkbox_unchecked.svg",
        ":/dark/checkbox_checked.svg",
        ":/dark/radio_unchecked.svg",
        ":/dark/radio_checked.svg",
    });

    for(const auto &key: fileList){
        QFileInfo fileInfo(key);
        if (tempDir.exists(fileInfo.fileName())){
            tempDir.remove(fileInfo.fileName());
        }

        QFile file(fileInfo.filePath());
        QString newFilePath = tempDir.filePath(fileInfo.fileName());
        file.copy(newFilePath);

        QFile newfile(newFilePath);
        newfile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

        newfile.open(QIODevice::ReadWrite); // open for read and write

        QByteArray fileData = newfile.readAll(); // read all the data into the byte array
        QString text(fileData); // add to text string for easy string replace
        text.replace(QString("#3DAEE9"), colorPalette.value("mainColor").name()); // replace text in string

        newfile.seek(0); // go to the beginning of the file
        newfile.write(text.toUtf8()); // write the new text back to the file

        newfile.close(); // close the file handle.

        stylesheet.replace(key, newFilePath);
    }
}
