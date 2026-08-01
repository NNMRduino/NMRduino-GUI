#include "twinleafcurrentsupplybipolar.h"
#include "ui_twinleafcurrentsupplybipolar.h"

#include <QMessageBox>
#include <QDebug>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QThread>
#include <QSettings>
#include <QComboBox>

#include "mainapplication.h"

using namespace Dev;

TwinleafCSB::TwinleafCSB(QWidget *parent, QString devName):
    Dev::GeneralDevice(parent, devName, DevType::TwinleafCSB)
  , qSerialPort(new QSerialPort(this))
  , ui(new Ui::TwinleafCSB)
{
    ui->setupUi(this);

    Q_FOREACH(QAbstractSpinBox* sp, findChildren<QAbstractSpinBox*>()) {
            sp->installEventFilter( this );
            sp->setFocusPolicy( Qt::StrongFocus );
    }

    Q_FOREACH(QComboBox* sp, findChildren<QComboBox*>()) {
            sp->installEventFilter( this );
            sp->setFocusPolicy( Qt::StrongFocus );
    }

    connect(ui->pushButton_reconnect, &QPushButton::clicked, this, &TwinleafCSB::reconnectDevice);
    connect(ui->pushButton_disconnect, &QPushButton::clicked, this, &TwinleafCSB::disconnectDevice);

    connect(ui->doubleSpinBox_x, &QDoubleSpinBox::editingFinished,
            this, [=] () {setParameter(Channel::X, CurrentParam::Offset, ui->doubleSpinBox_x->value());});
    connect(ui->doubleSpinBox_y, &QDoubleSpinBox::editingFinished,
            this, [=] () {setParameter(Channel::Y, CurrentParam::Offset, ui->doubleSpinBox_y->value());});
    connect(ui->doubleSpinBox_z, &QDoubleSpinBox::editingFinished,
            this, [=] () {setParameter(Channel::Z, CurrentParam::Offset, ui->doubleSpinBox_z->value());});

    connect(ui->doubleSpinBox_modAmpX, &QDoubleSpinBox::editingFinished,
            this, [=] () {setParameter(Channel::X, CurrentParam::ModAmplitude, ui->doubleSpinBox_modAmpX->value());});
    connect(ui->doubleSpinBox_modAmpY, &QDoubleSpinBox::editingFinished,
            this, [=] () {setParameter(Channel::Y, CurrentParam::ModAmplitude, ui->doubleSpinBox_modAmpY->value());});
    connect(ui->doubleSpinBox_modAmpZ, &QDoubleSpinBox::editingFinished,
            this, [=] () {setParameter(Channel::Z, CurrentParam::ModAmplitude, ui->doubleSpinBox_modAmpZ->value());});

    connect(ui->doubleSpinBox_modFreX, &QDoubleSpinBox::editingFinished,
            this, [=] () {setParameter(Channel::X, CurrentParam::ModFrequency, ui->doubleSpinBox_modFreX->value());});
    connect(ui->doubleSpinBox_modFreY, &QDoubleSpinBox::editingFinished,
            this, [=] () {setParameter(Channel::Y, CurrentParam::ModFrequency, ui->doubleSpinBox_modFreY->value());});
    connect(ui->doubleSpinBox_modFreZ, &QDoubleSpinBox::editingFinished,
            this, [=] () {setParameter(Channel::Z, CurrentParam::ModFrequency, ui->doubleSpinBox_modFreZ->value());});
}

TwinleafCSB::~TwinleafCSB(){
    disconnectDevice();
}

void TwinleafCSB::reconnectDevice(bool skipWarnings){
    auto isAvailable = false;
    QString portName = "";
    QString serialNumber;

    foreach (const QSerialPortInfo &serialPortInfo, QSerialPortInfo::availablePorts()) {
        if(!serialPortInfo.hasVendorIdentifier() || !serialPortInfo.hasProductIdentifier()){
           continue;
        }
        if(serialPortInfo.vendorIdentifier() != ui->spinBox_vendorID->value()){
           continue;
        }

        if(serialPortInfo.productIdentifier() != ui->spinBox_productID->value()){
           continue;
        }

        serialNumber = ui->lineEdit_serialNumber->text();
        if((serialPortInfo.serialNumber() != serialNumber) && !serialNumber.isEmpty()){
           continue;
        }

        portName = serialPortInfo.portName();

        isAvailable = true;

        printResponse("Twinleaf CSB found.");
        printResponse("Port name:" +  portName);
        printResponse("Vendor Id:" +  QString::number(ui->spinBox_vendorID->value()));
        printResponse("Product Id:" + QString::number(ui->spinBox_productID->value()));
        printResponse("Serial number:" + QString(serialPortInfo.serialNumber()));

        break;
    }

    if(!isAvailable){
        if(!skipWarnings){
            QMessageBox::warning(this, "Port error:", QString("Cannot connect to \"%1\".").arg(deviceName));
        }
        printResponse("Cannot connect Twinleaf CSB.");
        return;
    }

    // open and configure serialport
    qSerialPort->setPortName(portName);
    qSerialPort->open(QSerialPort::ReadWrite);
    qSerialPort->setBaudRate(QSerialPort::Baud115200);
    qSerialPort->setDataBits(QSerialPort::Data8);
    qSerialPort->setParity(QSerialPort::NoParity);
    qSerialPort->setStopBits(QSerialPort::OneStop);
    qSerialPort->setFlowControl(QSerialPort::NoFlowControl);

    printResponse("Connected.");

    previousCommands.clear();
}

void TwinleafCSB::disconnectDevice(){
    if(qSerialPort->isOpen()){
        if(qSerialPort->waitForReadyRead(200)){
            qSerialPort->readAll();
        }
        qSerialPort->close();
    }
    printResponse("Disconnected.");
}

void TwinleafCSB::saveSettings(QSettings &settings) const{
    GeneralDevice::saveSettings(settings);

    settings.setValue("VendorId", ui->spinBox_vendorID->value());
    settings.setValue("ProductId", ui->spinBox_productID->value());
    settings.setValue("SerialNumber", ui->lineEdit_serialNumber->text());

    settings.setValue("OffsetX", ui->doubleSpinBox_x->value());
    settings.setValue("OffsetY", ui->doubleSpinBox_y->value());
    settings.setValue("OffsetZ", ui->doubleSpinBox_z->value());

    settings.setValue("ModulationAmplitudeX", ui->doubleSpinBox_modAmpX->value());
    settings.setValue("ModulationAmplitudeY", ui->doubleSpinBox_modAmpY->value());
    settings.setValue("ModulationAmplitudeZ", ui->doubleSpinBox_modAmpZ->value());

    settings.setValue("ModulationFrequencyX", ui->doubleSpinBox_modFreX->value());
    settings.setValue("ModulationFrequencyY", ui->doubleSpinBox_modFreY->value());
    settings.setValue("ModulationFrequencyZ", ui->doubleSpinBox_modFreZ->value());

    settings.setValue("SerialCommand", ui->lineEdit_serialCommand->text());
}

void TwinleafCSB::loadSettings(const QSettings &settings, const App::LoadOptions loadOptions){
    GeneralDevice::loadSettings(settings, loadOptions);

    ui->spinBox_vendorID->setValue(settings.value("VendorId").toInt());
    ui->spinBox_productID->setValue(settings.value("ProductId").toInt());
    ui->lineEdit_serialNumber->setText(settings.value("SerialNumber").toString());

    if (loadOptions & App::LoadOption::ReconnectDevices){
        reconnectDevice(loadOptions & App::LoadOption::SkipWarnings);
    }

    setParameter(Channel::X, CurrentParam::Offset, settings.value("OffsetX").toDouble());
    setParameter(Channel::Y, CurrentParam::Offset, settings.value("OffsetY").toDouble());
    setParameter(Channel::Z, CurrentParam::Offset, settings.value("OffsetZ").toDouble());

    setParameter(Channel::X, CurrentParam::ModAmplitude, settings.value("ModulationAmplitudeX").toDouble());
    setParameter(Channel::Y, CurrentParam::ModAmplitude, settings.value("ModulationAmplitudeY").toDouble());
    setParameter(Channel::Z, CurrentParam::ModAmplitude, settings.value("ModulationAmplitudeZ").toDouble());

    setParameter(Channel::X, CurrentParam::ModFrequency, settings.value("ModulationFrequencyX").toDouble());
    setParameter(Channel::Y, CurrentParam::ModFrequency, settings.value("ModulationFrequencyY").toDouble());
    setParameter(Channel::Z, CurrentParam::ModFrequency, settings.value("ModulationFrequencyZ").toDouble());

    ui->lineEdit_serialCommand->setText(settings.value("SerialCommand").toString());
}

QStringList TwinleafCSB::getChannels() const{
    return channels.keys();
}

const QList<QSharedPointer<TwinleafSG> > TwinleafCSB::getTwinleafSGs() const
{
    return twinleafSGs;
}

void TwinleafCSB::setParameter(TwinleafCSB::Channel channel, const CurrentParam parameter, const double value){
    switch(parameter){
    case CurrentParam::Offset:
        switch (channel){
        case Channel::X:    ui->doubleSpinBox_x->setValue(value);       break;
        case Channel::Y:    ui->doubleSpinBox_y->setValue(value);       break;
        case Channel::Z:    ui->doubleSpinBox_z->setValue(value);       break;
        case Channel::None:                                             break;
        }
        break;
    case CurrentParam::ModAmplitude:
        switch (channel){
        case Channel::X:    ui->doubleSpinBox_modAmpX->setValue(value); break;
        case Channel::Y:    ui->doubleSpinBox_modAmpY->setValue(value); break;
        case Channel::Z:    ui->doubleSpinBox_modAmpZ->setValue(value); break;
        case Channel::None:                                             break;
        }
        break;
    case CurrentParam::ModFrequency:
        switch (channel){
        case Channel::X:    ui->doubleSpinBox_modFreX->setValue(value); break;
        case Channel::Y:    ui->doubleSpinBox_modFreY->setValue(value); break;
        case Channel::Z:    ui->doubleSpinBox_modFreZ->setValue(value); break;
        case Channel::None:                                             break;
        }
        if (value == 0.0){
            setParameter(channel, CurrentParam::ModAmplitude, 0.0);
        }
        break;
    }

    QString command = generateCommand(channel, parameter);

    QByteArray fullCommand = "";
    fullCommand.append((command + " " + QString::number(value) + "\r\n").toLatin1());

    if ((previousCommands.contains(command)) and (previousCommands.value(command) == value)){
        printCommand(fullCommand + " (already set)");
        return;
    }

    previousCommands.insert(command, value);

    write(fullCommand);
}

TwinleafCSB::Channel TwinleafCSB::getChannel(const QString channel) const
{
    return channels.value(channel, Channel::None);
}

void TwinleafCSB::setParameter(const QString channel, Dev::CSB::CurrentParam parameter, const double value){
    setParameter(getChannel(channel), parameter, value);
}

QString TwinleafCSB::generateCommand(const TwinleafCSB::Channel channel, const Dev::CSB::CurrentParam parameter){
    QString command;
    switch(channel){
    case Channel::X:
        switch(parameter){
        case Dev::CSB::CurrentParam::Offset:         command = "coil.x.current";                break;
        case Dev::CSB::CurrentParam::ModAmplitude:   command = "coil.x.modulation.amplitude";   break;
        case Dev::CSB::CurrentParam::ModFrequency:   command = "coil.x.modulation.frequency";   break;
        }
        break;
    case Channel::Y:
        switch(parameter){
        case Dev::CSB::CurrentParam::Offset:         command = "coil.y.current";                break;
        case Dev::CSB::CurrentParam::ModAmplitude:   command = "coil.y.modulation.amplitude";   break;
        case Dev::CSB::CurrentParam::ModFrequency:   command = "coil.y.modulation.frequency";   break;
        }
        break;
    case Channel::Z:
        switch(parameter){
        case Dev::CSB::CurrentParam::Offset:         command = "coil.z.current";                break;
        case Dev::CSB::CurrentParam::ModAmplitude:   command = "coil.z.modulation.amplitude";   break;
        case Dev::CSB::CurrentParam::ModFrequency:   command = "coil.z.modulation.frequency";   break;
        }
        break;
    case Channel::None:
        return "";
    }
    return command;
}

void TwinleafCSB::write(const QByteArray command){
    printCommand(command);
    if (!qSerialPort->isWritable()){
        printResponse("Cannot write to CSB.");
        return;
    }

    qSerialPort->write(command);
    for(int i=0; !qSerialPort->canReadLine() and (i < 10); i++){
        qSerialPort->waitForReadyRead(20);
        QThread::msleep(10);
    }
    QString response = qSerialPort->readLine(256);
    printResponse(response);
}

void TwinleafCSB::printCommand(QString command){
    if(ui->plainTextEdit->toPlainText().length() > 4096){
        ui->plainTextEdit->clear();
    }

    auto stringNoEscape = QString("<font color=%1>%2</font").arg(sendColor.name()).arg(command);
    stringNoEscape.replace("\n", "\\n").replace("\r", "\\r");
    ui->plainTextEdit->appendHtml(stringNoEscape);
    //qDebug() << command;
}

void TwinleafCSB::printResponse(QString response){
    if(ui->plainTextEdit->toPlainText().length() > 4096){
        ui->plainTextEdit->clear();
    }

    auto stringNoEscape = QString("<font color=%1>%2</font").arg(receiveColor.name()).arg(response);
    stringNoEscape.replace("\n", "\\n").replace("\r", "\\r");
    ui->plainTextEdit->appendHtml(stringNoEscape);
    //qDebug() << response;
}

//void TwinleafCSB::updateWidgetValues(){
//}

void TwinleafCSB::on_pushButton_send_clicked(){
    auto command = ui->lineEdit_serialCommand->text();
    command.replace("\\n", "\n").replace("\\r", "\r");
    write(command.toUtf8());
}

const QMap<QString, TwinleafCSB::Channel> TwinleafCSB::channels = QMap<QString, TwinleafCSB::Channel>(
        {{"x", Channel::X}, {"y", Channel::Y}, {"z", Channel::Z}});
