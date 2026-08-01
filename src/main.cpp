#include "mainapplication.h"

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <iostream>

QScopedPointer<QFile>   m_logFile;

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
    using ::endl;
}
#endif


void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

int main(int argc, char *argv[])
{
    QApplication::setSetuidAllowed(true);
    QApplication app(argc, argv);

    QApplication::setApplicationName("NMRduino");
    QApplication::setApplicationVersion(App::MainApplication::version);
    QApplication::setOrganizationName("NMRduino Project");

    //QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // Set the logging file
    // check which a path to file you use
    QDir configDir = App::MainApplication::getConfigLocation().path();
    QString logFileName = QString("_logFile_%2.txt").arg(QDateTime::currentDateTime().toString("yyyy-MM"));
    QString logFilePath = configDir.filePath(logFileName);

    m_logFile.reset(new QFile(logFilePath));

    // Open the file logging
    m_logFile.data()->open(QFile::Append | QFile::Text);

    // Set handler
    qInstallMessageHandler(messageHandler);

    App::MainApplication application;
    return app.exec();
}

// The implementation of the handler
// (see https://evileg.com/en/post/154/ "Qt/C++ - Lesson 050. Logging Qt application events to a text file)"
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";

    // Write the date of recording
    QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ");
    QString message = QString("%1 (%2:%3, %4)").arg(msg, file, QString(context.line), function);

    switch(type){
        case QtInfoMsg:     message = date + "INF " + message; break;
        case QtDebugMsg:    message = date + "DBG " + message; break;
        case QtWarningMsg:  message = date + "WRN " + message; break;
        case QtCriticalMsg: message = date + "CRT " + message; break;
        case QtFatalMsg:    message = date + "FTL " + message; break;
    }

    QTextStream debug_console_out (stdout);

    // By type determine to what level belongs message
    debug_console_out << message << Qt::endl;
    if (type != QtDebugMsg){
        // Open stream file writes
        QTextStream out(m_logFile.data());
        out << message << Qt::endl;
        out.flush();    // Clear the buffered data
    }
}

