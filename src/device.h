#ifndef DEVICE_H
#define DEVICE_H

#include <QWidget>
#include <QPointer>
#include <QDebug>
#include <functional>

#include "misc.h"

class QSettings;
class PulseSequence;

namespace Dev{
    Q_NAMESPACE
    class DeviceManager;
    class GeneralDevice;

    class DAQ;
	class CSB;
}

namespace App {
    class MainApplication;
    enum class LoadOption; Q_DECLARE_FLAGS(LoadOptions, LoadOption)
}

namespace Task {
    enum class RunMode; Q_DECLARE_FLAGS(RunModes, RunMode)
}

namespace Ui {
    class DeviceManager;
}


class Dev::GeneralDevice : public QWidget
{
    Q_OBJECT
    friend class Dev::DeviceManager;
public:
    enum class DevType {
        None                =  0,
        TestDAQ             =  1,
        TeensyPulse4        =  2,
		TwinleafCSB         =  3,
    }; Q_ENUM(DevType)

    explicit GeneralDevice(QWidget *parent=nullptr, QString devName="", DevType devType=DevType::None);
    virtual ~GeneralDevice() {}

    virtual void saveSettings(QSettings &settings) const;
    virtual void loadSettings(const QSettings &, App::LoadOptions) {}
    virtual void setDefaultSettings() {}

    bool eventFilter( QObject* o, QEvent* e);

public:
    const QString deviceName;

private:
    const DevType deviceType;
    static const QMap<Dev::GeneralDevice::DevType, QString> devTypesMap;
};


// Interfaces
class Dev::CSB
{
public:
    enum class CurrentParam {
        Offset,
        ModAmplitude,
        ModFrequency,
    };

    explicit CSB() {}
    virtual ~CSB() {}
    virtual QStringList getChannels() const {return QStringList();}
    virtual void setParameter(const QString, const CurrentParam, const double) {}
    virtual double getParameter(const QString, const CurrentParam) const {return 0.0;}
};

class Dev::DAQ
{
public:
    enum class Mode{
        None       = -1,
        Buffered   =  0,
        Unbuffered =  1,
    };

    //ToDo: Refactor split pulse generator/awg and data acquisition parts into two different interfaces
    enum class ChannelType {
        None           = -1,
        Analog         =  0,
        Digital        =  1,
        DigitalBipolar =  2,
    };

    explicit DAQ() {}
    virtual ~DAQ() {}

    virtual void setMode(Mode) {}
    virtual void setMode(const QString) {}

    virtual void setSampleRate(double) {}
    virtual double getSampleRate() const {return 0.0;}

    virtual void setSupersamplingFactor(uint8_t) {}
    virtual uint8_t getSupersamplingFactor() const {return 0;}

    virtual void setNumberOfSamples(int) {}
    virtual int getNumberOfSamples() const {return 0;}
    virtual const QList<int> getAllowedNumberOfSamples() const {return QList<int>();}

    //virtual double getAcquisitionTime() const;

    virtual void abortAcquisition() {}

    virtual QVector<quint16> get16BitBuffer(int=-1) {return QVector<quint16>();}
    virtual QVector<quint32> get32BitBuffer(int=-1) {return QVector<quint32>();}

    virtual QMap<int, QPair<QVector<double>, QVector<double>>> getPlotData(int length, bool reversed=false);
    QMap<int, QPair<QVector<double>, QVector<double>>> getPlotData(bool reversed=false);

    virtual int uploadPulseSequence(QSharedPointer<PulseSequence>) {return 0;}

    virtual QStringList getModes() const {return QStringList();}

    virtual ChannelType getChannelType(const QPair<const QString, const QString>) const {return ChannelType::None;}
    virtual ChannelType getChannelType(const QString) const {return ChannelType::None;}

    virtual int acquireData() {return -1;}
    virtual int acquireData(std::function<void(int)>) {return -1;}
};


class Dev::DeviceManager : public QWidget
{
    Q_OBJECT
public:
    DeviceManager(QWidget* parent, App::MainApplication * const app);
    ~DeviceManager();

    void saveSettings(QSettings &settings) const;
    void loadSettings(QSettings &settings, App::LoadOptions loadOptions);

    QSharedPointer<Dev::GeneralDevice> createDevice(QString devType, QString devName);

    void removeDevice(QWeakPointer<Dev::GeneralDevice> device);

    static QStringList getDevTypes();

    QSharedPointer<Dev::GeneralDevice> getDevice(const QString devName="") const;
    template<typename T> QSharedPointer<T> getDevice(const QString devName="") const;

    QList<QSharedPointer<Dev::GeneralDevice>> getDevices() const;
    template<typename T> QList<QSharedPointer<T>> getDevices() const;

    template<typename T> QStringList getDeviceNames() const;

    void disableGuiElements(Task::RunModes runMode);
    void enableGuiElements();

private:
    QSharedPointer<Dev::GeneralDevice> createDevice(GeneralDevice::DevType devType, QString devName);

private:
    QList<QSharedPointer<Dev::GeneralDevice>> devices;
    QScopedPointer<Ui::DeviceManager> ui;
    const QPointer<App::MainApplication> app;

signals:
    void devicesChanged();
};

extern template QSharedPointer<Dev::DAQ>  Dev::DeviceManager::getDevice<Dev::DAQ>(const QString devName) const;
extern template QSharedPointer<Dev::CSB>  Dev::DeviceManager::getDevice<Dev::CSB>(const QString devName) const;

extern template QList<QSharedPointer<Dev::DAQ>>  Dev::DeviceManager::getDevices<Dev::DAQ>() const;
extern template QList<QSharedPointer<Dev::CSB>>  Dev::DeviceManager::getDevices<Dev::CSB>() const;

extern template QStringList Dev::DeviceManager::getDeviceNames<Dev::DAQ>() const;
extern template QStringList Dev::DeviceManager::getDeviceNames<Dev::CSB>() const;

#endif // DEVICE_H
