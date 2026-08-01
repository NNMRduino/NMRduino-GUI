#include "testdaq.h"
#include "ui_teensydataacquisition.h"

#include <QTimer>
#include <math.h>
#include <QDebug>

using namespace Dev;

TestDAQ::TestDAQ(QWidget *parent, QString devName, GeneralDevice::DevType devType):
    Dev::GeneralDevice(parent, devName, devType)
    , ui(new Ui::TeensyDataAcquisition)
    , timer(new QTimer)
{
    ui->setupUi(this);

    connect(ui->doubleSpinBox_sampleRate, &QDoubleSpinBox::editingFinished,
            this, [=] () {mode = Mode::Buffered; setSampleRate(ui->doubleSpinBox_sampleRate->value());});
    connect(ui->comboBox_dataSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=] () {setNumberOfSamples(ui->comboBox_dataSize->currentData().toInt());});

    timer->setInterval(20);
    connect(timer.data(), &QTimer::timeout, this, [=] () {for(int i=0; i<40; i++){readData();}});
}

TestDAQ::~TestDAQ(){

}

void TestDAQ::saveSettings(QSettings &settings) const
{
    GeneralDevice::saveSettings(settings);

    settings.setValue("SampleRate",      getSampleRate());
    settings.setValue("NumberOfSamples", getNumberOfSamples());
    settings.setValue("Supersampling", getSupersamplingFactor());
}

void TestDAQ::loadSettings(const QSettings &settings, App::LoadOptions loadOptions)
{
    GeneralDevice::loadSettings(settings, loadOptions);
    setSampleRate(settings.value("SampleRate", 2000.0).toDouble());
    setNumberOfSamples(settings.value("NumberOfSamples", 2048).toInt());
    setSupersamplingFactor(settings.value("Supersampling", 0).toInt());
}

void TestDAQ::setMode(Dev::DAQ::Mode newMode){
    mode = newMode;
}

void TestDAQ::setMode(QString newMode){
    mode = pulseModes.key(newMode, Mode::None);
}

QStringList TestDAQ::getModes() const{
    return pulseModes.values();
}

void TestDAQ::setNumberOfSamples(int n){
    numberOfSamples = n;

    auto oldState = ui->comboBox_dataSize->blockSignals(true);
    ui->comboBox_dataSize->setCurrentIndex(ui->comboBox_dataSize->findData(n));
    ui->comboBox_dataSize->blockSignals(oldState);

    ui->doubleSpinBox_acqTime->setValue( numberOfSamples / getSampleRate());
}

int TestDAQ::getNumberOfSamples() const{
    return numberOfSamples;
}

const QList<int> TestDAQ::getAllowedNumberOfSamples() const
{
    //return QList<int>({2048, 4096, 8192, 16384, 32768, 65536});
    return QList<int>({2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536});
}

void TestDAQ::setSampleRate(double newSampleRate)
{
    ui->doubleSpinBox_sampleRate->setValue(newSampleRate);
    ui->doubleSpinBox_acqTime->setValue( numberOfSamples / getSampleRate());
}

double TestDAQ::getSampleRate() const
{
    return ui->doubleSpinBox_sampleRate->value();
}

QVector<quint16> TestDAQ::get16BitBuffer(int length)
{
    return data.read(length);
}

QMap<int, QPair<QVector<double>, QVector<double> > > TestDAQ::getPlotData(int length, bool reversed)
{
    auto tData = timeData.mid(0, length);
    if (reversed){
        return {{0, {tData, voltageData.readReversed(length)}}};
    } else {
        return {{0, {tData, voltageData.read(length)}}};
    }
}

void TestDAQ::abortAcquisition(){
    timer->stop();
}

void TestDAQ::setDefaultSettings()
{
    ui->comboBox_dataSize->clear();
    for(auto &dataLengthItem: getAllowedNumberOfSamples()){
        ui->comboBox_dataSize->addItem(QString::number(dataLengthItem), dataLengthItem);
    }
    setNumberOfSamples(8192);

    setSampleRate(2000);
}

void TestDAQ::readData()
{
    auto dt = timeData.value(1) - timeData.value(0);
    auto t = dt * data.numberPointsWritten();

    qint16 dataPoint = (1 << 10) * sin(t * 73.127 * 2 * PI) + (1 << 11) + (rand() >> 4);

    data.append(dataPoint);

    double voltage = dataPoint;
    if(voltage > 32767.5){
        voltage -= 65536.0; // offset for bipolar input
    }

    voltage *= 0.000152587890625;// signal amplitude in volts // 5.0V/32768.0
    //qDebug() << voltage;
    voltageData.append(voltage);

    if (data.numberPointsWritten() == numberOfSamplesRequested){
        QTimer::singleShot(int(ui->doubleSpinBox_acqTime->value()*1000), this, [=] () {emit acquisitionFinished(0);});
    }
}

int TestDAQ::acquireData(){
    numberOfSamplesRequested = numberOfSamples;

    if(numberOfSamplesRequested < 1){
        qCritical() << "Datalength is less than 1";
        return -1;
    }

    data.resize(numberOfSamplesRequested);
    data.reset();

    voltageData.resize(numberOfSamplesRequested);
    voltageData.reset();

    double dt = 1.0 / getSampleRate();
    timeData.resize(numberOfSamplesRequested);
    std::generate(timeData.begin(), timeData.end(), [n=0, &dt]() mutable { return n++ * dt;});

    switch(mode){
    case Mode::Buffered:
        for(auto i=0; i<numberOfSamplesRequested; i++){
            readData();
        }
        break;
    case Mode::Unbuffered:
        timer->setInterval(20);
        timer->start();
        //qDebug() << "Test3";
        break;
    case Mode::None:
        break;
    }

    return 0;
}

int TestDAQ::acquireData(std::function<void (int)> callback)
{
    disconnect(receiverConnection);
    receiverConnection = connect(this, &Dev::TestDAQ::acquisitionFinished,
                                [=] (const int err) {disconnect(receiverConnection); callback(err);});
    return acquireData();
}

const QMap<DAQ::Mode, QString> TestDAQ::pulseModes = QMap<DAQ::Mode, QString>({
    {DAQ::Mode::Buffered,   "Buffered"},
    {DAQ::Mode::Unbuffered, "Unbuffered"},
});
