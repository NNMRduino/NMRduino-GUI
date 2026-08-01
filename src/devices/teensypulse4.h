#ifndef TeensyPulse44_H
#define TeensyPulse44_H

#include <QPointer>
#include <QTimer>

#include "device.h"
#include "misc.h"

template <typename T>
class QVector;
class QSerialPort;

namespace Dev {
    class TeensyPulse4;
}

namespace Ui {
    class TeensyPulse4;
}

class Dev::TeensyPulse4 : public Dev::GeneralDevice, public Dev::DAQ
{
    Q_OBJECT
    Q_PROPERTY(QColor sendColor MEMBER sendColor)
    Q_PROPERTY(QColor receiveColor MEMBER receiveColor)
public:
    enum class PulseMode{
        None                             =  -1,
        RealTimeMonitor                  =0x0A,
        PulseSequenceUpload              =0x0C,
    }; Q_ENUM(PulseMode)

    enum class Channel{
        None                             = -1,
        D0                               =  0,
        D1                               =  1,
        D2                               =  2,
        A0                               =  3,
        A1                               =  4,
        ACQ                              =  5,
        DT                               =  6,
        A0_DAC                           =  7,
        A1_DAC                           =  8,
    }; Q_ENUM(Channel)

    explicit TeensyPulse4(QWidget *parent=nullptr, QString deviceName="",
                                   Dev::GeneralDevice::DevType devType=Dev::GeneralDevice::DevType::TeensyPulse4);
    virtual ~TeensyPulse4() override;

    void saveSettings(QSettings &settings) const override;
    void loadSettings(const QSettings &settings, App::LoadOptions loadOptions) override;
    void setDefaultSettings() override;

    void setMode(Mode newMode) override;
    void setMode(QString newMode) override;
    QStringList getModes() const override;

    void setNumberOfSamples(int n) override;
    int getNumberOfSamples() const override;
    const QList<int> getAllowedNumberOfSamples() const override;

    void setSampleRate(double sampleRate) override;
    double getSampleRate() const override;

    void abortAcquisition() override;

    QVector<quint16> get16BitBuffer(int length) override;
    QMap<int, QPair<QVector<double>, QVector<double>>> getPlotData(int length, bool reversed) override;

    int uploadPulseSequence(QSharedPointer<PulseSequence>) override;

    virtual ChannelType getChannelType(const QPair<const QString, const QString> channel) const override;
    virtual ChannelType getChannelType(const QString channel) const override;

    void reconnectDevice(bool skipWarnings=false);
    void disconnectDevice();

private:    
    virtual QByteArray generateSerialCommand();
    void readData();

    void printCommand(QString command);
    void printResponse(QString command);

    int acquireData(QByteArray serialCommand);
    void setPulseMode(PulseMode newPulseMode);

    void updateStatus(QString newStatus);

public slots:
    int acquireData() override;
    int acquireData(std::function<void(int)>) override;

signals:
    void acquisitionFinished(int err);

private slots:
    void receiveTimeout();

private:
    QScopedPointer<QSerialPort> qSerialPort;

    uint32_t hardwareTimer;
    int dataLength, dataLengthRequested;

    QMetaObject::Connection receiverConnection;

    QScopedPointer<Ui::TeensyPulse4> ui;

    QColor sendColor, receiveColor;

    CircularBuffer<quint16> data;
    CircularBuffer<double> voltageData;
    QVector<double> timeData;

    PulseMode pulseMode;

    QTimer receiveTimeoutTimer;

    static const QMap<PulseMode, QString> pulseModes;
    static const QMap<Channel, QString> channels;
};
#endif // TeensyPulse44_H
