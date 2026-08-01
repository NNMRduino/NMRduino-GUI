#include "device.h"
#include "mainapplication.h"

#include <QDebug>
#include <QSettings>
#include <QSpinBox>
#include <QComboBox>
#include <QEvent>
#include <QInputDialog>

#include "ui_devicemanager.h"

#ifdef MAIN_ZULF
#include "devices/mainzulf.h"
#endif


#include "devices/testdaq.h"
#include "devices/teensypulse4.h"
#include "devices/twinleafcurrentsupplybipolar.h"


#ifdef USE_NI_VISA
#include "devices/siglentawg.h"
#endif

#ifdef USE_PI_THERMOMETER
#include "devices/pithermometer.h"
#endif

#include "tasks.h"

using namespace Dev;

GeneralDevice::GeneralDevice(QWidget *parent, QString devName, DevType devType) :
    QWidget(parent),
    deviceName(devName),
    deviceType(devType){
}


void GeneralDevice::saveSettings(QSettings &settings) const{
    settings.setValue("DeviceName", deviceName);
    settings.setValue("DeviceType", devTypesMap.value(deviceType));
}

bool GeneralDevice::eventFilter(QObject *o, QEvent *e){
    //if (e->type() == QEvent::Wheel && qobject_cast<QAbstractSpinBox*>(o))        {
    if (e->type() == QEvent::Wheel &&
            (qobject_cast<QAbstractSpinBox*>(o) || qobject_cast<QComboBox*>(o)))        {
        e->ignore();
        return true;
    }
    return QWidget::eventFilter(o, e);
}

const QMap<GeneralDevice::DevType, QString> GeneralDevice::devTypesMap = QMap<GeneralDevice::DevType, QString>(
{
    {GeneralDevice::DevType::TestDAQ,           "Test DAQ"},
    {GeneralDevice::DevType::TeensyPulse4,      "Teensy Pulse 4"},
	{GeneralDevice::DevType::TwinleafCSB,       "Twinleaf CSB"},
});


//double DAQ::getAcquisitionTime() const{
//    return getNumberOfSamples() / getSampleRate() * (getSupersamplingFactor() + 1);
//}

QMap<int, QPair<QVector<double>, QVector<double> > > DAQ::getPlotData(int, bool)
{
    return {{0, {QVector<double>(), QVector<double>()}}};
}

QMap<int, QPair<QVector<double>, QVector<double> > > DAQ::getPlotData(bool reversed)
{
    return getPlotData(-1, reversed);
}



DeviceManager::DeviceManager(QWidget *parent, App::MainApplication * const mainapp):
    QWidget(parent), ui(new Ui::DeviceManager), app(mainapp)
{
    ui->setupUi(this);
    delete ui->toolBox_devices->currentWidget();
}

DeviceManager::~DeviceManager()
{

}

void DeviceManager::saveSettings(QSettings &settings) const
{
    settings.beginGroup("DeviceManager");
    {
        settings.setValue("ActiveDeviceIndex", ui->toolBox_devices->currentIndex());

        QStringList listDeviceNames;
        for(auto &device: qAsConst(devices)){
            listDeviceNames.append(device->deviceName);
        }
        settings.setValue("DeviceNames", listDeviceNames);
    }
    settings.endGroup();

    for(auto &device: qAsConst(devices)){
        settings.beginGroup(device->deviceName);
        device->saveSettings(settings);
        settings.endGroup();
    }
}
void DeviceManager::loadSettings(QSettings &settings, App::LoadOptions loadOptions)
{
    if (loadOptions & App::LoadOption::ReconnectDevices)    {
        for(auto &device: qAsConst(devices)){
            removeDevice(device);
        }

        for(auto &deviceName: settings.value("DeviceManager/DeviceNames").toStringList()) {
            settings.beginGroup(deviceName);
            auto deviceType = GeneralDevice::devTypesMap.key(settings.value("DeviceType").toString());
            settings.endGroup();
            createDevice(deviceType, deviceName);
        }
    }

    for(auto device: qAsConst(devices)){
        settings.beginGroup(device->deviceName);
        device->loadSettings(settings, loadOptions);
        settings.endGroup();
    }

    if(loadOptions & App::LoadOption::LoadActiveIndices){
        ui->toolBox_devices->setCurrentIndex(settings.value("DeviceManager/ActiveDeviceIndex").toInt());
    }
}

QSharedPointer<GeneralDevice> DeviceManager::createDevice(GeneralDevice::DevType devType,
                                                          QString devName){
    QSharedPointer<GeneralDevice> device;

    for(auto &device: qAsConst(devices)){
        if(device.isNull()){
            devices.removeAll(device);
        }
    }

    switch(devType){
    case GeneralDevice::DevType::None:
        break;
    case GeneralDevice::DevType::TestDAQ:
        device = QSharedPointer<TestDAQ>::create(this, devName);
        break;
    case GeneralDevice::DevType::TeensyPulse4:
        device = QSharedPointer<TeensyPulse4>::create(this, devName);
        break;
	case GeneralDevice::DevType::TwinleafCSB:
        device = QSharedPointer<TwinleafCSB>::create(this, devName);
        break;
    }

    if (device.isNull()){
        return device;
    }

    qDebug() << device->deviceName << device->deviceType;
    device->setDefaultSettings();
    devices.append(device);
    ui->toolBox_devices->addItem(device.data(), device->deviceName);
    emit devicesChanged();
    return device;
}

QSharedPointer<GeneralDevice> DeviceManager::createDevice(QString devType, QString devName){
    if (devName.isEmpty()){
        bool ok;
        devName = QInputDialog::getText(this, "Adding a new device", "Please choose a name:", QLineEdit::Normal,
                                           "", &ok, Qt::MSWindowsFixedSizeDialogHint);
        if(not ok){
            return QSharedPointer<GeneralDevice>();
        }
    }
    auto device = createDevice(GeneralDevice::devTypesMap.key(devType, GeneralDevice::DevType::None), devName);

    if (device.isNull()){
        return device;
    }


    QSettings settings(app->getSettingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(devName);
    device->loadSettings(settings, App::LoadOption::ReconnectDevices);
    settings.endGroup();

    return device;
}

void DeviceManager::removeDevice(QWeakPointer<GeneralDevice> device){
    devices.removeAll(device.lock());
    device.clear();
    emit devicesChanged();
}


QStringList DeviceManager::getDevTypes(){
    return GeneralDevice::devTypesMap.values();
}

void DeviceManager::disableGuiElements(Task::RunModes runMode){
    for(auto &device: qAsConst(devices)){
        if (device.isNull()){
            continue;
        }

        device->setEnabled(true);

        if (runMode & Task::RunMode::DevicesDisabled){
           device->setEnabled(false);
           continue;
        }

        if (runMode & Task::RunMode::DAQDisabled and not device.dynamicCast<DAQ>().isNull()){
            device->setEnabled(false);
            continue;
        }
    }
}

void DeviceManager::enableGuiElements(){
    for(auto &device: qAsConst(devices)){
        if (not device.isNull()){
            device->setEnabled(true);
        }
    }
}

QList<QSharedPointer<Dev::GeneralDevice>> DeviceManager::getDevices() const{
      QList<QSharedPointer<Dev::GeneralDevice>> devicesList;

      for(auto &device: qAsConst(devices)){
          if (not device.isNull()){
            devicesList.append(device);
          }
      }

      return devicesList;
}

QSharedPointer<GeneralDevice> DeviceManager::getDevice(const QString devName) const
{
    for(auto &device: qAsConst(devices)){
        if (device.isNull()){
            continue;
        }

        if ((devName.isEmpty() or device->deviceName == devName)){
            return device;
        }
    }
    return QSharedPointer<GeneralDevice>();
}

template<typename T>
QSharedPointer<T> DeviceManager::getDevice(const QString devName) const{
    for(auto &device: qAsConst(devices)){
        if (not device.dynamicCast<T>().isNull()
                and (devName.isEmpty() or device->deviceName == devName)){
            return device.dynamicCast<T>();
        }
    }
    return QSharedPointer<T>();
}

template QSharedPointer<DAQ>  DeviceManager::getDevice<DAQ>(QString devName) const;
template QSharedPointer<CSB>  DeviceManager::getDevice<CSB>(QString devName) const;

template<typename T>
QList<QSharedPointer<T>> DeviceManager::getDevices() const
{
    QList<QSharedPointer<T>> devicesList;

    for(auto &device: qAsConst(devices)){
        auto deviceCast = device.dynamicCast<T>();
        if (not deviceCast.isNull()){
            devicesList.append(deviceCast);
        }
    }

    return devicesList;
}

template QList<QSharedPointer<DAQ>>  DeviceManager::getDevices<DAQ>() const;
template QList<QSharedPointer<CSB>>  DeviceManager::getDevices<CSB>() const;

template<typename T>
QStringList DeviceManager::getDeviceNames() const
{
    QStringList deviceNames;
    for(auto &device: qAsConst(devices)){
        if(device.isNull()){
            continue;
        }

        if(device.dynamicCast<T>().isNull()){
            continue;
        }

        deviceNames.append(device->deviceName);
    }
    return deviceNames;
}

template QStringList DeviceManager::getDeviceNames<DAQ>() const;
template QStringList DeviceManager::getDeviceNames<CSB>() const;
