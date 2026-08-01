#ifndef MISC_H
#define MISC_H

#include <QString>
#include <QList>

const double PI = 3.141592653589793238460;

const double E_GYROMAGNETIC_RATIO  =  2*PI*28024000000; // 2 Pi 28.024 GHz/T
const double H_GYROMAGNETIC_RATIO  =  2*PI*   42577479; // 2 Pi 42.577 MHz/T
const double C_GYROMAGNETIC_RATIO  =  2*PI*   10708400; // 2 Pi 10.708 MHz/T
const double O_GYROMAGNETIC_RATIO  = -2*PI*    5772000; // 2 Pi -5.772 MHz/T
const double F_GYROMAGNETIC_RATIO  =  2*PI*   40052000; // 2 Pi 40.052 MHz/T
const double H2_GYROMAGNETIC_RATIO =  2*PI*    6536000; // 2 Pi  6.536 MHz/T

//#define MAX_NUMBER_PULSES 4096
//#define MAX_NUMBER_PULSES 21000
#define MAX_NUMBER_PULSES 15000

class QDir;
class QWidget;

bool makeDataDirectory(QDir &dir, QString dataDirectory, QString cwd=QString("."), QWidget *qWidget = nullptr);

QString incrementPathName(QString oldPathName);

QDir getDataDirectory(QString dataDirectory, QString cwd=QString("."));



void delay(unsigned int msecs=200);

class PulseSequence
{

public:
    explicit PulseSequence();
    int32_t getChannelData(int n, QString channel);
    int getDuration(int n);
    int getNumberOfPulses();
    QList<QString> getChannels();
    void clear();
    void addPulse(int pulseLength_us, QMap<QString, int32_t> channelData);
    bool isValid();

private:
    int pulseLength[MAX_NUMBER_PULSES];
    QList<QMap<QString, int32_t>> channelDataList;
    QList<QString> channels;
};

template<typename T>
class CircularBuffer
{
public:
    CircularBuffer<T>();
    void resize(int size);
    void append(const T &value);
    void append(const QVector<T> value);
    int size() const;
    int numberPointsWritten() const;
    void clear();
    void reset();
    void resetReadIndex();
    T readNext();
    QVector<T> read(int numberOfElements=-1);
    QVector<T> readReversed(int numberOfElements=-1);
private:
    int writeIndex, readIndex, nPoints;
    QVector<T> data;
};

extern template class CircularBuffer<quint16>;
extern template class CircularBuffer<quint32>;
extern template class CircularBuffer<double>;

void dataWriteToFile(QString Filename, QByteArray data);
void dataWriteToFile(const QString filename, CircularBuffer<unsigned short> data);
void dataWriteToFile(const QString filename, QVector<quint16> data);
void dataWriteToFile(const QString filename, QVector<quint32> data);

#endif // MISC_H
