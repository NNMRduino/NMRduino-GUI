#ifndef TWINLEAFCURRENTSUPPLYBIPOLAR_H
#define TWINLEAFCURRENTSUPPLYBIPOLAR_H

#include <QMap>
#include <QPointer>

#include "device.h"

class QSerialPort;


namespace Dev {
    class TwinleafCSB;
    class TwinleafSG;
}

namespace Ui {
    class TwinleafCSB;    
}

class Dev::TwinleafCSB : public Dev::GeneralDevice, public Dev::CSB
{
    Q_OBJECT
    Q_PROPERTY(QColor receiveColor MEMBER receiveColor)
    Q_PROPERTY(QColor sendColor MEMBER sendColor)
public:
    enum class Channel{
        None,
        X,
        Y,
        Z,
    }; Q_ENUM(Channel)

    explicit TwinleafCSB(QWidget *parent=nullptr, QString deviceName="");
    virtual ~TwinleafCSB() override;

    void setParameter(const QString channel, const Dev::CSB::CurrentParam parameter, const double value) override;
    //double getParameter(const QString channel, const Dev::CSB::CurrentParam parameter) const override;

    void reconnectDevice(bool skipWarnings=false);
    void disconnectDevice();

    void saveSettings(QSettings &settings) const override;
    void loadSettings(const QSettings &settings, const App::LoadOptions loadOptions) override;

    QStringList getChannels() const override;
    const QList<QSharedPointer<Dev::TwinleafSG>> getTwinleafSGs() const;

private:
    void setParameter(Channel channel, const Dev::CSB::CurrentParam parameter, const double value);
    Channel getChannel(const QString channel) const;
    QString generateCommand(Channel channel, const Dev::CSB::CurrentParam parameter);

    void write(const QByteArray command);
    void printCommand(QString command);
    void printResponse(QString response);

    const QList<QSharedPointer<Dev::TwinleafSG>> twinleafSGs;

    //void updateWidgetValues();

private slots:
    void on_pushButton_send_clicked();

private:
    QScopedPointer<QSerialPort> qSerialPort;
    QScopedPointer<Ui::TwinleafCSB> ui;

    static const QMap<QString, Channel> channels;
    QColor sendColor, receiveColor;

    QMap<QString, double> previousCommands;
};
#endif // TWINLEAFCURRENTSUPPLYBIPOLAR_H
