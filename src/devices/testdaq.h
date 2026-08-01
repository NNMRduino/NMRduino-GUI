#ifndef TESTDAQ_H
#define TESTDAQ_H

#include <QWidget>
#include <QSettings>

#include <device.h>

namespace Dev {
    class TestDAQ;
}

namespace Ui {
    class TeensyDataAcquisition;
}
class Dev::TestDAQ: public Dev::GeneralDevice, public Dev::DAQ
{
    Q_OBJECT
    Q_PROPERTY(QColor sendColor MEMBER sendColor)
    Q_PROPERTY(QColor receiveColor MEMBER receiveColor)
public:
    TestDAQ(QWidget *parent=nullptr, QString deviceName="",
            Dev::GeneralDevice::DevType devType=Dev::GeneralDevice::DevType::TestDAQ);
    ~TestDAQ();

    void saveSettings(QSettings &settings) const override;
    void loadSettings(const QSettings &settings, App::LoadOptions loadOptions) override;

    void setMode(Mode newMode) override;
    void setMode(QString newMode) override;
    QStringList getModes() const override;

    void setNumberOfSamples(int n) override;
    int getNumberOfSamples() const override;
    const QList<int> getAllowedNumberOfSamples() const override;

    void setSampleRate(double sampleRate) override;
    double getSampleRate() const override;

    QVector<quint16> get16BitBuffer(int length) override;
    QMap<int, QPair<QVector<double>, QVector<double>>> getPlotData(int length, bool reversed) override;

    void abortAcquisition() override;

    void setDefaultSettings() override;

    virtual void doesNothing() {}

private:
    void printCommand(QString command);
    void printResponse(QString command);




private slots:
    void readData();

signals:
    void acquisitionFinished(int err);

public slots:
    int acquireData() override;
    int acquireData(std::function<void(int)>) override;

private:
    int numberOfSamples, numberOfSamplesRequested;
    QMetaObject::Connection receiverConnection;
    Mode mode;

    CircularBuffer<quint16> data;
    CircularBuffer<double> voltageData;
    QVector<double> timeData;

    QColor sendColor, receiveColor;

    QScopedPointer<Ui::TeensyDataAcquisition> ui;

    static const QMap<Mode, QString> pulseModes;

    const QScopedPointer<QTimer> timer;
};

#endif // TESTDAQ_H
