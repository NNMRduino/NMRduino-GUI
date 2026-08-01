VERSION = 3.37.3
QT       += core gui serialport printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++14

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \   
    devices/teensypulse4.cpp \
    devices/testdaq.cpp \
    devices/twinleafcurrentsupplybipolar.cpp \
    plots/fft.cpp \
    plots/fftplot.cpp \
    plots/filters.cpp \
    main.cpp \
    mainapplication.cpp \
    mainwindow.cpp \
    misc.cpp \
    device.cpp \
    plots/myqcustomplot.cpp \
    plots/timedomainplot.cpp \
    plots/qcustomplot.cpp \
    spoiler.cpp \
    tasks.cpp \
    tasks/pulseacquire.cpp \
    tasks/spectrumanalyzer.cpp \
    tasks/taskscheduler.cpp \

HEADERS += \
    devices/teensypulse4.h \
    devices/testdaq.h \
    devices/twinleafcurrentsupplybipolar.h \
    plots/fft.h \
    plots/fftplot.h \
    plots/filters.h \
    plots.h \
    plots/myqcustomplot.h \
    plots/timedomainplot.h \
    spoiler.h \
    tasks.h \
    tasks/pulseacquire.h \
    mainapplication.h \
    mainwindow.h \
    misc.h \
    device.h \
    plots/qcustomplot.h \
    tasks/spectrumanalyzer.h \
    tasks/taskscheduler.h \

FORMS += \
    devicemanager.ui \
    devices/teensydataacquisition.ui \
    devices/teensypulse4.ui \
    devices/twinleafcurrentsupplybipolar.ui \
    plots/fftplot.ui \
    plots/timedomainplot.ui \
    taskmanager.ui \
    taskqueue.ui \
    tasks/pulseacquire.ui \
    tasks/spectrumanalyzer.ui \
    tasks/taskscheduler.ui \
    mainwindow.ui \

RESOURCES +=\
    qrc/breeze.qrc


windows {
    DEFINES += \
        WINVER=0x0600 \
        _WIN32_WINNT=0x0600 \
        QCUSTOMPLOT_USE_OPENGL

    QT += opengl
}

unix {
    LIBS += -L/usr/lib -lrt -lpthread

    INCLUDEPATH += /usr/local/include
}

TARGET = nmrduino

DEFINES += APP_VERSION=\\\"$$VERSION\\\"

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:RC_ICONS += icon.ico
win32-g++: QMAKE_LFLAGS += -static-libgcc -static-libstdc++
