#include "teensypulse4.h"
#include "ui_teensypulse4.h"

#include <QMessageBox>
#include <QDebug>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QThread>
#include <QSettings>
#include <QTimer>
#include <math.h>

#include "misc.h"
#include "mainapplication.h"

using namespace Dev;

const double ClockSpeed = 150.0 * 1000.0 * 1000.0; // 150 MHz

TeensyPulse4::TeensyPulse4(QWidget *parent, QString devName, GeneralDevice::DevType devType):
    Dev::GeneralDevice(parent, devName, devType)
    , qSerialPort(new QSerialPort(parent))
    , ui(new Ui::TeensyPulse4)
{
    ui->setupUi(this);

    // ToDo
    ui->label_15->setVisible(false);
    ui->label_10->setVisible(false);
    ui->label_8->setVisible(false);
    ui->doubleSpinBox_sampleRate->setVisible(false);
    ui->comboBox_dataSize->setVisible(false);
    ui->doubleSpinBox_acqTime->setVisible(false);

    Q_FOREACH(QAbstractSpinBox* sp, findChildren<QAbstractSpinBox*>()) {
            sp->installEventFilter( this );
            sp->setFocusPolicy( Qt::StrongFocus );
    }

    Q_FOREACH(QComboBox* sp, findChildren<QComboBox*>()) {
            sp->installEventFilter( this );
            sp->setFocusPolicy( Qt::StrongFocus );
    }

    ui->comboBox_dataSize->clear();
    foreach(auto dataLengthItem, TeensyPulse4::getAllowedNumberOfSamples()){
        ui->comboBox_dataSize->addItem(QString::number(dataLengthItem), dataLengthItem);
    }

    connect(ui->doubleSpinBox_sampleRate, &QDoubleSpinBox::editingFinished,
            //this, [=] () {mode = Mode::Buffered; setSampleRate(ui->doubleSpinBox_sampleRate->value());});
            this, [=] () {setSampleRate(ui->doubleSpinBox_sampleRate->value());});
    connect(ui->comboBox_dataSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=] () {setNumberOfSamples(ui->comboBox_dataSize->currentData().toInt());});

    connect(qSerialPort.data(),        &QSerialPort::readyRead, this, &TeensyPulse4::readData);
    connect(ui->pushButton_reconnect,  &QPushButton::clicked,   this, &TeensyPulse4::reconnectDevice);
    connect(ui->pushButton_disconnect, &QPushButton::clicked,   this, &TeensyPulse4::disconnectDevice);

    connect(&receiveTimeoutTimer, &QTimer::timeout, this, &TeensyPulse4::receiveTimeout);
}

TeensyPulse4::~TeensyPulse4(){
    disconnectDevice();
}

void TeensyPulse4::saveSettings(QSettings &settings) const{
    GeneralDevice::saveSettings(settings);

    settings.setValue("SampleRate",      getSampleRate());
    settings.setValue("NumberOfSamples", getNumberOfSamples());
    settings.setValue("Supersampling", getSupersamplingFactor());

    settings.setValue("ProductId", ui->spinBox_productId->value());
    settings.setValue("VendorId", ui->spinBox_vendorId->value());
    settings.setValue("SerialNumber", ui->lineEdit_serialNumber->text());

    //settings.setValue("PulseMode", pulseModes.value(pulseMode));
}

void TeensyPulse4::loadSettings(const QSettings &settings, App::LoadOptions loadOptions){
    GeneralDevice::loadSettings(settings, loadOptions);

    setSampleRate(settings.value("SampleRate", 2000.0).toDouble());
    setNumberOfSamples(settings.value("NumberOfSamples", 2048).toInt());
    setSupersamplingFactor(settings.value("Supersampling", 0).toInt());

    ui->spinBox_productId->setValue(settings.value("ProductId", 0).toInt());
    ui->spinBox_vendorId->setValue(settings.value("VendorId", 0).toInt());
    ui->lineEdit_serialNumber->setText(settings.value("SerialNumber", 0).toString());

    if (loadOptions & App::LoadOption::ReconnectDevices){
        reconnectDevice(loadOptions & App::LoadOption::SkipWarnings);
    }

    setSampleRate(settings.value("SampleRate", 2000.0).toDouble());

    //setMode(settings.value("PulseMode").toString());
}

void TeensyPulse4::setDefaultSettings()
{

}

void TeensyPulse4::setNumberOfSamples(int n){
    dataLength = n;

    auto oldState = ui->comboBox_dataSize->blockSignals(true);
    ui->comboBox_dataSize->setCurrentIndex(ui->comboBox_dataSize->findData(n));
    ui->comboBox_dataSize->blockSignals(oldState);

    ui->doubleSpinBox_acqTime->setValue( dataLength / getSampleRate());
}

int TeensyPulse4::getNumberOfSamples() const{
    return dataLength;
}

const QList<int> TeensyPulse4::getAllowedNumberOfSamples() const{
    return QList<int>({512, 1024, 2048, 4096, 8192, 16384, 32768, 65536});
}

void TeensyPulse4::setSampleRate(double newSampleRate){
    hardwareTimer = static_cast<uint32_t>(std::round(ClockSpeed / newSampleRate) - 1);
    auto sampleRate = getSampleRate();

    auto oldState = ui->doubleSpinBox_sampleRate->blockSignals(true);
    ui->doubleSpinBox_sampleRate->setValue(sampleRate);
    ui->doubleSpinBox_sampleRate->blockSignals(oldState);

    ui->doubleSpinBox_acqTime->setValue(dataLength / sampleRate);
}

double TeensyPulse4::getSampleRate() const{
    return ClockSpeed / (hardwareTimer + 1);
}

int TeensyPulse4::acquireData(){
    return acquireData(generateSerialCommand());
}

int TeensyPulse4::acquireData(std::function<void (int)> callback)
{
    if (pulseMode == PulseMode::RealTimeMonitor){
        printResponse("pulseMode not supported for buffered acquisition.");
        return -1;
    }

    disconnect(receiverConnection);
    receiverConnection = connect(this, &Dev::TeensyPulse4::acquisitionFinished,
                                [=] (const int err) {disconnect(receiverConnection); callback(err);});
    return acquireData();
}

int TeensyPulse4::acquireData(QByteArray serialCommand){
    if (!qSerialPort->isWritable() or !qSerialPort->isOpen()){
        printResponse("Cannot write to data recorder.");
        updateStatus("Teensy is not writable.");
        return -1;
    }

    if(dataLength < 1){
        qCritical() << "Datalength is less than 1.";
        updateStatus("Datalength is less than 1.");
        return -1;
    }

    data.resize(dataLength);
    data.reset();

    voltageData.resize(dataLength);
    voltageData.reset();

    timeData.resize(dataLength);

    qSerialPort->readAll();
    if (!qSerialPort->isWritable() or !qSerialPort->isOpen()){
        printResponse("Cannot write to data recorder.");
        return -1;
    }    

    switch(pulseMode){
    case PulseMode::PulseSequenceUpload:
        dataLengthRequested = dataLength;
        receiveTimeoutTimer.stop();
        receiveTimeoutTimer.setInterval(1000);
        updateStatus(QString("Acquiring %1 data points...").arg(dataLengthRequested));
        break;
    case PulseMode::RealTimeMonitor:
    {
        double dt = 1.0 / getSampleRate();
        int n = 0;
        std::generate(timeData.begin(), timeData.end(), [&n, &dt]() { return n++ * dt;});
        dataLengthRequested = 1;
        updateStatus("Acquiring unbuffered data...");
        break;
    }
    case PulseMode::None:
        dataLengthRequested = 0;
        qCritical() << "Unknown mode: Mode::None";
        updateStatus("Unknown mode: Mode::None.");
        return -1;
    }

    qSerialPort->write(serialCommand);
    qSerialPort->waitForBytesWritten();
    printCommand(serialCommand.toHex(' '));
    return 0;
}

void TeensyPulse4::updateStatus(QString newStatus)
{
    ui->lineEdit_status->setText(newStatus);
}

void TeensyPulse4::setPulseMode(PulseMode newPulseMode)
{
    pulseMode = newPulseMode;
}

void TeensyPulse4::receiveTimeout(){
    qCritical() << "Acquisition failed.";
    dataLengthRequested = 0;
    receiveTimeoutTimer.stop();
    updateStatus(QString("Timed out. (Received %1 data points.)").arg(data.numberPointsWritten()));
    emit acquisitionFinished(-1);
}

void TeensyPulse4::readData(){
    if (dataLengthRequested < 1){
        return;
    }

    if((pulseMode ==PulseMode::PulseSequenceUpload) and (not receiveTimeoutTimer.isActive())){
        receiveTimeoutTimer.start();
    }

    char byteData[2];
    while(qSerialPort->bytesAvailable() > 1){
        qSerialPort->read(byteData, 2);

        qint16 dataPoint = (   (static_cast<unsigned short>(static_cast<unsigned char>(byteData[0])) << 8)
                             + (static_cast<unsigned short>(static_cast<unsigned char>(byteData[1])) << 0) ) & 0xFFFF;

        // On the Raspberry Pi "char" is unsigned by default. On the most other systems it is signed.
        data.append(dataPoint);

        double voltage = dataPoint;

        voltage *= 0.000152587890625;// signal amplitude in volts // 5.0V/32768.0;
        voltageData.append(voltage);
    }

    if(dataLengthRequested > 1){
        updateStatus(QString("Received %1/%2 data points.").arg(data.numberPointsWritten()).arg(dataLengthRequested));
    }

    if (pulseMode==PulseMode::PulseSequenceUpload){
        if (data.numberPointsWritten() == dataLengthRequested){
            receiveTimeoutTimer.stop();
            qDebug() << "Acquisition successful.";
            dataLengthRequested = 0;
            emit acquisitionFinished(0);
        }
    }
}

void TeensyPulse4::reconnectDevice(bool skipWarnings){
    disconnectDevice();
    QString portName;
    QString serialNumber;

    bool isAvailable = false;

    foreach (const QSerialPortInfo &serialPortInfo, QSerialPortInfo::availablePorts()) {
       if(!serialPortInfo.hasVendorIdentifier() or !serialPortInfo.hasProductIdentifier()){
           continue;
       }
       if(serialPortInfo.vendorIdentifier() != ui->spinBox_vendorId->value()){
           continue;
       }

       if(serialPortInfo.productIdentifier() != ui->spinBox_productId->value()){
           continue;
       }

       serialNumber = ui->lineEdit_serialNumber->text();
       if((serialPortInfo.serialNumber() != serialNumber) && !serialNumber.isEmpty()){
          continue;
       }

      portName = serialPortInfo.portName();
      isAvailable = true;

      printResponse("ArduinoDAQ has been found:");
      printResponse("Vendor Id:"  +  QString::number(ui->spinBox_vendorId->value()));
      printResponse("Product Id:" + QString::number(ui->spinBox_productId->value()));
      printResponse("Serial Number:" +serialNumber);

      break;
    }

    if(!isAvailable){
        if (!skipWarnings){
            QMessageBox::warning(this, "Port error:", QString("Cannot connect to \"%1\".").arg(deviceName));
        }

        printResponse(QString("Cannot connect to \"%1\". Available devices:").arg(deviceName));
        foreach (const QSerialPortInfo &serialPortInfo, QSerialPortInfo::availablePorts()) {
           if(!serialPortInfo.hasVendorIdentifier() or !serialPortInfo.hasProductIdentifier()){
               continue;
           }
           printResponse("Vendor ID:     " + QString::number(serialPortInfo.vendorIdentifier()));
           printResponse("Product ID:    " + QString::number(serialPortInfo.productIdentifier()));
           printResponse("Serial number: " + serialPortInfo.serialNumber());
           printResponse("");
        }
        return;
    }

    // open and configure QSerialPort
    qSerialPort->setPortName(portName);
    qSerialPort->open(QSerialPort::ReadWrite);
    qSerialPort->setBaudRate(QSerialPort::Baud115200);
    qSerialPort->setDataBits(QSerialPort::Data8);
    qSerialPort->setParity(QSerialPort::NoParity);
    qSerialPort->setStopBits(QSerialPort::OneStop);
    qSerialPort->setFlowControl(QSerialPort::NoFlowControl);

    printResponse("Connected.");
    updateStatus("Connected.");
}

void TeensyPulse4::disconnectDevice()
{
    if(qSerialPort->isOpen()){
        qSerialPort->close();
    }
    printResponse("Disconnected.");
    updateStatus("Disconnected.");
}

void TeensyPulse4::abortAcquisition(){
    if (!qSerialPort->isWritable() || !qSerialPort->isOpen()){
        printResponse("Cannot write to data recorder.");
        updateStatus("Cannot write to Teensy.");
        return;
    }

    dataLengthRequested = 0;

    QByteArray serialData;

    qSerialPort->write(QByteArray(2, '\0'));
    qSerialPort->waitForBytesWritten();

    // Wait for response with 20 * 50ms = 1s timeout
    for(int i=0; !serialData.endsWith(QByteArray(2, '\0')) and (i < 20); i++){
        qSerialPort->waitForReadyRead(50);
        serialData += qSerialPort->readAll();
        QThread::msleep(50);
    }

    if (!serialData.endsWith(QByteArray(2, '\0'))){
        printResponse("Aborting acquisition wasn't successful.");
        printResponse(serialData.toHex());
        updateStatus("Aborting acquisition wasn't successful.");
    } else {
        printResponse("Acquisition stopped!");
        updateStatus("Acquisition stopped.");
    }
}

QVector<quint16> TeensyPulse4::get16BitBuffer(int length)
{
    return data.read(length);
}

QMap<int, QPair<QVector<double>, QVector<double> > > TeensyPulse4::getPlotData(int length, bool reversed)
{
    if (length == -1){
        length = timeData.length();
    }

    auto tData = timeData.mid(0, length);
    if (reversed){
        return {{0, {tData, voltageData.readReversed(length)}}};
    } else {
        return {{0, {tData, voltageData.read(length)}}};
    }
}

void TeensyPulse4::printCommand(QString command){
    if(ui->plainTextEdit->toPlainText().length() > 4096){
        ui->plainTextEdit->clear();
    }

    auto stringNoEscape = QString("<font color=%1>%2</font").arg(sendColor.name(), command);
    stringNoEscape.replace("\n", "\\n").replace("\r", "\\r");
    ui->plainTextEdit->appendHtml(stringNoEscape);
    qDebug() << command;
}

void TeensyPulse4::printResponse(QString response){
    if(ui->plainTextEdit->toPlainText().length() > 4096){
        ui->plainTextEdit->clear();
    }

    auto stringNoEscape = QString("<font color=%1>%2</font").arg(receiveColor.name(), response);
    stringNoEscape.replace("\n", "\\n").replace("\r", "\\r");
    ui->plainTextEdit->appendHtml(stringNoEscape);
    qDebug() << response;
}

void TeensyPulse4::setMode(Mode newMode){
    switch(newMode){
    case Mode::None:
    case Mode::Buffered:
        pulseMode = PulseMode::None;
        break;
    case Mode::Unbuffered:
        setPulseMode(PulseMode::RealTimeMonitor);
        break;
    }
}

QByteArray TeensyPulse4::generateSerialCommand(){
    QByteArray serialCommand;

    serialCommand.resize(18);

    serialCommand[0] = '\xA5'; // start
    serialCommand[1] = static_cast<char>(pulseMode);
    serialCommand[2]  = 0x00;
    serialCommand[3]  = 0x00;
    serialCommand[4]  = 0x00;
    serialCommand[5]  = 0x00;
    serialCommand[6]  = 0x00;
    serialCommand[7]  = 0x00;
    serialCommand[8]  = 0x00;
    serialCommand[9]  = 0x00;
    serialCommand[10] = 0x00;
    serialCommand[11] = 0x00;
    serialCommand[12] = 0x00;
    serialCommand[13] = 0x00;
    serialCommand[14] = 0x00;
    serialCommand[15] = 0x00;
    serialCommand[16] = 0x00;

    serialCommand[17] = '\x5A'; // end

    switch(pulseMode){
    case PulseMode::RealTimeMonitor:
        if(hardwareTimer > 0xFFFF){
            printResponse(QString("Hardware timer (0x%1) too big! Set to maximum value 0xFFFF instead.").arg(
                QString::number(hardwareTimer, 16)));
            hardwareTimer = 0xFFFF;
            setSampleRate(getSampleRate());
        }
        serialCommand[10] = static_cast<char>((static_cast<unsigned int>(hardwareTimer) >>8) & 0xFF);
        serialCommand[11] = static_cast<char>( static_cast<unsigned int>(hardwareTimer)      & 0xFF);
        break;
    case PulseMode::PulseSequenceUpload:
        serialCommand[2] = static_cast<char>((dataLength)/512);  // Requires new Teensy code
        if (hardwareTimer > 0x1FFFF){
            printResponse(QString("Hardware timer (0x%1) too big! Set to maximum value 0x1FFFF instead.").arg(
                QString::number(hardwareTimer, 16)));
            hardwareTimer = 0x1FFFF;
        }
        serialCommand[10] = static_cast<char>((static_cast<unsigned int>(hardwareTimer) >>8) & 0xFF);
        serialCommand[11] = static_cast<char>( static_cast<unsigned int>(hardwareTimer)      & 0xFF);
        break;
    case PulseMode::None:
        qCritical() << "pulseMode is None.";
        break;
    }

    return serialCommand;
}


void TeensyPulse4::setMode(const QString newMode) {
    setPulseMode(pulseModes.key(newMode, PulseMode::None));
}


DAQ::ChannelType TeensyPulse4::getChannelType(const QPair<const QString, const QString> channel) const{
    if (channel.first != deviceName){
         return ChannelType::None;
    }

    return getChannelType(channel.second);
}

DAQ::ChannelType TeensyPulse4::getChannelType(const QString channel)const{
    switch(channels.key(channel, Channel::None)){
    case Channel::A0:
    case Channel::A1:
        return ChannelType::Analog;
    case Channel::D0:
    case Channel::D1:
    case Channel::D2:
    case Channel::A0_DAC:
    case Channel::A1_DAC:
        return ChannelType::Digital;
    case Channel::None:
    case Channel::ACQ:
    case Channel::DT:
        return ChannelType::None;
    }
    return  ChannelType::None;
}

QStringList TeensyPulse4::getModes() const{
    return pulseModes.values();
}

int TeensyPulse4::uploadPulseSequence(QSharedPointer<PulseSequence> pulseSequence){
    switch(pulseMode){
    case PulseMode::None:
    case PulseMode::RealTimeMonitor:
        qDebug() << "Pulse sequence upload skipped for current pulse mode:" << pulseMode;
        return 0;
    case PulseMode::PulseSequenceUpload:
        break;
    }

    if (!qSerialPort->isWritable() || !qSerialPort->isOpen()){
        printResponse("Cannot write to data recorder.");
        return -1;
    }

    int numberOfPulses = pulseSequence->getNumberOfPulses(); // Teensy uses event-timestamp instead of pulses
                                                             // Last pulse duration is ignored!!!
    if (!pulseSequence->isValid()){
        printResponse("Error: Pulse sequence is invalid.");
        return -1;
    }

    int numberTransmittingPulses;
    int numberPulsesSent = 0;
    unsigned int timePosition = 0;
    uint32_t dataBytes;
    char arduinoResponse[1];
    const int overheadBytes = 8;
    const int bytesPerPulse = 8;

    const uint32_t acqWord  = 0x0000000A;
    const uint32_t dacWord  = 0x0000000D;
    const uint32_t gpioWord = 0x00000000;

    QByteArray serialPulseData;

    dataLengthRequested = 0;

    int32_t numberOfAcquisitionEvents = 0;
    double dt = 0.0;

    while(numberPulsesSent < numberOfPulses){
        // Send maximal 7 pulses per serial command (USB buffer is limited to 64 bytes)
        numberTransmittingPulses = std::min((64-overheadBytes)/bytesPerPulse, numberOfPulses - numberPulsesSent);

        serialPulseData.resize(overheadBytes + bytesPerPulse*numberTransmittingPulses);
        serialPulseData[0] = static_cast<char>(0xA5);
        serialPulseData[1] = static_cast<char>(0x81);  // Mode for pulse sequence transfer
        serialPulseData[2] = static_cast<char>(overheadBytes + bytesPerPulse*numberTransmittingPulses);
        serialPulseData[3] = static_cast<char>(numberOfPulses >> 8);
        serialPulseData[4] = static_cast<char>(numberOfPulses);
        serialPulseData[5] = static_cast<char>(numberPulsesSent >> 8);
        serialPulseData[6] = static_cast<char>(numberPulsesSent);
        serialPulseData[overheadBytes + bytesPerPulse*numberTransmittingPulses - 1] = static_cast<char>(0x5A);

        for(int i=0; i < numberTransmittingPulses; i++){
            serialPulseData[overheadBytes - 1 + i*bytesPerPulse + 0] = static_cast<char>((timePosition >> 24) & 0xFF);
            serialPulseData[overheadBytes - 1 + i*bytesPerPulse + 1] = static_cast<char>((timePosition >> 16) & 0xFF);
            serialPulseData[overheadBytes - 1 + i*bytesPerPulse + 2] = static_cast<char>((timePosition >> 8 ) & 0xFF);
            serialPulseData[overheadBytes - 1 + i*bytesPerPulse + 3] = static_cast<char>((timePosition      ) & 0xFF);

            timePosition += static_cast<unsigned int>(pulseSequence->getDuration(numberPulsesSent));

            dataBytes = 0x00000000;

            int32_t value;
            for(auto &channel: channels){
                if(not pulseSequence->getChannels().contains(channel)){
                    continue;
                }

                value = pulseSequence->getChannelData(numberPulsesSent, channel);
                auto channelEnum = channels.key(channel);

                switch(channelEnum){
                // Start ADC Acquisition
                case Channel::ACQ:
                    if(value == 0){
                        continue;
                    } else if (value < 0){
                        printResponse("Negative number of acquisition events.");
                        return -1;
                    } else if (value > 65536){
                        printResponse("Too many acquisition events.");
                        return -1;
                    }
                    numberOfAcquisitionEvents += value;
                    dataBytes |= acqWord;
                    dataBytes |= (static_cast<uint16_t>(value-1) << 16);
                    break;
                case Channel::DT:
                {
                    if((dataBytes & 0xF) != acqWord){
                        continue;
                    }

                    uint32_t numberOfPoints = (dataBytes >> 16) + 1;
                    uint32_t acquisitionTime = numberOfPoints * 10 * value;
                    dt = value / 100000.0;
                    uint32_t eventDuration = pulseSequence->getDuration(numberPulsesSent);
                    if (eventDuration < acquisitionTime){
                        qCritical() << timePosition << eventDuration << acquisitionTime;
                        printResponse("Acquisition event too short.");
                        return -1;
                    }

                    dataBytes |= ((static_cast<uint16_t>(value) & 0xFFF) << 4);
                    break;
                }

                // Update SPI DAC output
                case Channel::A0_DAC:
                    if(value == 0){
                        continue;
                    } else if ((dataBytes & 0xF) == acqWord) {
                        continue;
                    }

                    dataBytes |= dacWord;
                    dataBytes |= ((static_cast<uint16_t>(value) & 0xFFF) << 8);
                    break;
                case Channel::A1_DAC:
                    if(value == 0){
                        continue;
                    } else if ((dataBytes & 0xF) == acqWord) {
                        continue;
                    }

                    dataBytes |= dacWord;
                    dataBytes |= ((static_cast<uint16_t>(value) & 0xFFF) << 20);
                    break;

                // Update GPIO output
                case Channel::D0:
                    if (((dataBytes & 0xF) == acqWord) or ((dataBytes & 0xF) == dacWord)){
                        continue;
                    }
                    dataBytes |= gpioWord;
                    if (value == 0){
                        dataBytes |= (0 << 25) | (0 << 24);
                    } else if (value > 0){
                        dataBytes |= (1 << 25) | (0 << 24);
                    } else if (value < 0) {
                        dataBytes |= (0 << 25) | (1 << 24);
                    }
                    break;

                case Channel::D1:
                    if (((dataBytes & 0xF) == acqWord) or ((dataBytes & 0xF) == dacWord)){
                        continue;
                    }
                    dataBytes |= gpioWord;
                    if (value == 0){
                        dataBytes |= (0 << 27) | (0 << 26);
                    }else if (value > 0){
                        dataBytes |= (1 << 27) | (0 << 26);
                    } else if (value < 0) {
                        dataBytes |= (0 << 27) | (1 << 26);
                    }
                    break;

                case Channel::D2:
                    if ((dataBytes & acqWord) or (dataBytes & dacWord)){
                        continue;
                    }
                    dataBytes |= gpioWord;
                    if (value == 0){
                        dataBytes |= (0 << 16) | (0 << 17);
                    }else if (value > 0){
                        dataBytes |= (1 << 16) | (0 << 17);
                    } else if (value < 0) {
                        dataBytes |= (0 << 16) | (1 << 17);
                    }
                    break;

                case Channel::A0:
                    if ((dataBytes & acqWord) or (dataBytes & dacWord)){
                        continue;
                    }
                    dataBytes |= gpioWord;
                    if (value == 0){
                        dataBytes |= (0 << 18) | (0 << 19);
                    }else if (value > 0){
                        dataBytes |= (1 << 18) | (0 << 19);
                    } else if (value < 0) {
                        dataBytes |= (1 << 18) | (1 << 19);
                    }
                    break;

                case Channel::A1:
                    if ((dataBytes & acqWord) or (dataBytes & dacWord)){
                        continue;
                    }
                    dataBytes |= gpioWord;
                    if (value == 0){
                        dataBytes |= (0 << 20) | (0 << 21);
                    }else if (value > 0){
                        dataBytes |= (1 << 20) | (0 << 21);
                    } else if (value < 0) {
                        dataBytes |= (1 << 20) | (1 << 21);
                    }
                    break;

                default:
                    printResponse("Error: Invalid output channel in uploadPulseSequence:" + channel);
                    continue;
                }
            }

            serialPulseData[overheadBytes - 1 + i*bytesPerPulse + 4]  = ((dataBytes >> 24) & 0xFF);
            serialPulseData[overheadBytes - 1 + i*bytesPerPulse + 5]  = ((dataBytes >> 16) & 0xFF);
            serialPulseData[overheadBytes - 1 + i*bytesPerPulse + 6]  = ((dataBytes >>  8) & 0xFF);
            serialPulseData[overheadBytes - 1 + i*bytesPerPulse + 7]  = ((dataBytes >>  0) & 0xFF);

            numberPulsesSent++;
        }

        updateStatus(QString("Transmitting pulse events %1/%2...").arg(numberPulsesSent).arg(numberOfPulses));

        qSerialPort->readAll();
        qSerialPort->write(serialPulseData);
        printCommand(serialPulseData.toHex(' '));
        qSerialPort->waitForBytesWritten();

        if(!qSerialPort->waitForReadyRead(100)){
            printResponse("Teensy timed out.");
            return -1;
        } else {
            qSerialPort->read(arduinoResponse, 1);
        }
    }

    setNumberOfSamples(numberOfAcquisitionEvents);
    int n = 0;
    std::generate(timeData.begin(), timeData.end(), [&n, &dt]() { return n++ * dt;});

    //if (numberOfAcquisitionEvents != getNumberOfSamples()){
    //    printResponse("Number of acquisition events does not match number of samples.");
    //    return -1;
    //}

    updateStatus("Pulse events transmitted.");
    return 0;
}

const QMap<TeensyPulse4::PulseMode, QString> TeensyPulse4::pulseModes =
        QMap<TeensyPulse4::PulseMode, QString>(
{
    //{TeensyPulse4::PulseMode::RealTimeMonitor,                       "0x0A Real time monitor"},
    {TeensyPulse4::PulseMode::PulseSequenceUpload,                   "0x0C Pulse sequence upload"},
});

const QMap<TeensyPulse4::Channel, QString> TeensyPulse4::channels =
        QMap<TeensyPulse4::Channel, QString>(
{
    {TeensyPulse4::Channel::ACQ,          "ACQ"},
    {TeensyPulse4::Channel::DT,           "dt"},
    {TeensyPulse4::Channel::A0_DAC,       "A0_dac"},
    {TeensyPulse4::Channel::A1_DAC,       "A1_dac"},
    {TeensyPulse4::Channel::D0,           "D0"},
    {TeensyPulse4::Channel::D1,           "D1"},
    {TeensyPulse4::Channel::D2,           "D2"},
    {TeensyPulse4::Channel::A0,           "A0"},
    {TeensyPulse4::Channel::A1,           "A1"},
});
