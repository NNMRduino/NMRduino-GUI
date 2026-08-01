#include "misc.h"
#include <QDebug>
#include <QMessageBox>
#include <QWidget>
#include <QTime>
#include <complex>
#include <QDir>
#include <QThread>
#include <QRegularExpression>

bool makeDataDirectory(QDir &dir, QString dataDirectory, QString cwd, QWidget *qWidget) {
    dir.setPath(QDir::cleanPath(getDataDirectory(dataDirectory, cwd).path()));

    if (dir.exists()){
        qDebug() << "Data directory:" << dir.path();
        return true;
    }

    qDebug() << "Create data directory...";
    if (!dir.mkpath(".")){
        QMessageBox::warning(qWidget, "Couldn't create directory:", dir.path());
        return false;
    }

    qDebug() << "Data directory:" << dir.path();
    return true;
}

QDir getDataDirectory(QString dataDirectory, QString cwd){
    QDir dir(cwd);
    dir.setPath(QDir::cleanPath(dir.absoluteFilePath(dataDirectory)));
    return dir;
}


void delay(unsigned int msecs){
    QThread::msleep(msecs);
}

PulseSequence::PulseSequence(){  
    clear();
}

int32_t PulseSequence::getChannelData(int n, QString channel){
    if (n >= getNumberOfPulses()){
        return 0;
    } else {
        return channelDataList[n].value(channel, 0);
    }
}

int PulseSequence::getDuration(int n){
    if (n >= getNumberOfPulses()){
        return -1;
    } else {
        return pulseLength[n];
    }
}

int PulseSequence::getNumberOfPulses(){
    return channelDataList.length();
}

QList<QString> PulseSequence::getChannels(){
    return channels;
}

void PulseSequence::clear(){
    channelDataList.clear();
    channels.clear();
}

void PulseSequence::addPulse(int pulseLength_us, QMap<QString, int32_t> channelData){
    if (pulseLength_us < 1){
        qDebug() << "pulse has to be >= 1 us:" << getNumberOfPulses() << pulseLength_us;
        return;
    } else if (pulseLength_us < 0){
        qDebug() << "Pulse has shorter than 2147483647 microseconds.";
        return;
    } else if (getNumberOfPulses() >= MAX_NUMBER_PULSES){
        qWarning() << "Too many pulses.";
        return;
    }

    pulseLength[getNumberOfPulses()] = pulseLength_us;

    channelDataList.append(channelData);

    auto keys = channelData.keys();
    foreach(QString channel, keys){
        if (!channels.contains(channel)){
            channels.append(channel);
        }
    }
}

bool PulseSequence::isValid(){
    int n = getNumberOfPulses();

    if (n > MAX_NUMBER_PULSES){
        qWarning() << "Too many pulses." << n;
        return false;
    }

    if (n < 1){
        qWarning() << "Sequence is empty.";
        return false;
    }

    foreach(QString key, channels){
        if (channelDataList[n-1].value(key, 0) == 0){
            continue;
        }

        if (key.contains("DAC")){
            qCritical() << "Last value is non-zero for DAC channel:" << key;
            continue;
        }

        qCritical() << "Last value is non-zero for at least one channel.";
        return false;
    }

    if (getDuration(n-1) < 1){
        qCritical() << "Last pulse segment is < 1 us.";
        return false;
    }

    return true;
}

template<typename T> CircularBuffer<T>::CircularBuffer() {
    clear();
}

template<typename T>
void CircularBuffer<T>::resize(int size){
    if(size<1){
        size=1;
    }
    data.resize(size);
    writeIndex %= size;
    readIndex %= size;
}

template<typename T>
void CircularBuffer<T>::append(const T &value){
    data.replace(writeIndex % size(), value);
    writeIndex++;
    writeIndex %= size();
    nPoints++;
}

template<typename T>
void CircularBuffer<T>::append(const QVector<T> value) {
    for(auto v: value){
        append(v);
    }
}

template<typename T>
int CircularBuffer<T>::size() const {
    return data.size();
}

template<typename T>
int CircularBuffer<T>::numberPointsWritten() const {
    return nPoints;
}

template<typename T>
void CircularBuffer<T>::clear() {
    data.clear();
    data.append(T());
    writeIndex = 0;
    readIndex = 0;
    nPoints = 0;
}

template<typename T>
void CircularBuffer<T>::reset() {
    writeIndex = readIndex;
    nPoints = 0;
}

template<typename T>
void CircularBuffer<T>::resetReadIndex() {
    readIndex = writeIndex;
}

template<typename T>
T CircularBuffer<T>::readNext(){
    auto returnValue = data.value(readIndex % size());
    readIndex++;
    readIndex %= size();
    return returnValue;
}

template<typename T>
QVector<T> CircularBuffer<T>::read(int numberOfElements){
    resetReadIndex();
    if (numberOfElements==-1) {
        numberOfElements = size();
    }
    auto returnVector = QVector<T>(numberOfElements);
    for (int i=0; i < numberOfElements; i++){
        returnVector.replace(i, readNext());
    }
    return returnVector;
}

template<typename T>
QVector<T> CircularBuffer<T>::readReversed(int numberOfElements){
    if (numberOfElements==-1) {
        numberOfElements = size();
    }

    readIndex = (writeIndex + (size() - numberOfElements) % size());

    auto returnVector = QVector<T>(numberOfElements);
    for (int i=numberOfElements; i > 0; i--){returnVector.replace(i-1, readNext());}
    return returnVector;}

template class CircularBuffer<quint16>;
template class CircularBuffer<quint32>;
template class CircularBuffer<double>;

void dataWriteToFile(QString Filename, QByteArray data){ // Write data to file
    QFile file(Filename);

    if(file.isOpen()){
        qWarning() << "Cannot open file for writing";
        return;
    }

    file.open(QIODevice::WriteOnly);
    int len=data.size();

    file.write(reinterpret_cast<char *>(&len), sizeof(len)); // ???
    file.write(data);
    file.flush();

    if(file.isOpen()){
        file.close();
    }
}


void dataWriteToFile(const QString filename, CircularBuffer<quint16> data){
    dataWriteToFile(filename, data.read());
}


void dataWriteToFile(const QString filename, QVector<quint16> data){
    QByteArray dataByteArray(data.size()*2, '\0');

    for (int i=0; i<data.size(); i++){
        dataByteArray[2*i]     = static_cast<char>(static_cast<unsigned char>((data.at(i) >> 8) & 0xFF));
        dataByteArray[2*i + 1] = static_cast<char>(static_cast<unsigned char>((data.at(i))      & 0xFF));
    }

    dataWriteToFile(filename, dataByteArray);
}

void dataWriteToFile(const QString filename, QVector<quint32> data){
    QByteArray dataByteArray(data.size()*4, '\0');

    for (int i=0; i<data.size(); i++){
        dataByteArray[4*i]     = static_cast<char>(static_cast<unsigned char>((data.at(i) >> 24) & 0xFF));
        dataByteArray[4*i + 1] = static_cast<char>(static_cast<unsigned char>((data.at(i) >> 16) & 0xFF));
        dataByteArray[4*i + 2] = static_cast<char>(static_cast<unsigned char>((data.at(i) >> 8)  & 0xFF));
        dataByteArray[4*i + 3] = static_cast<char>(static_cast<unsigned char>((data.at(i))       & 0xFF));
    }

    dataWriteToFile(filename, dataByteArray);
}

QString incrementPathName(QString oldPathName)
{
    QRegularExpressionMatch match;
    static QRegularExpression qRegularExpression(QString("(_(\\d+))?(/|\\\\)?$"));

    oldPathName.lastIndexOf(qRegularExpression, -1, &match);

    if (!match.hasMatch()){ // No previous number (_2, _3, ...); no backslash
        return  oldPathName.append("_1");
    } else if (match.captured(2).isNull()){ // No previous number (_2, _3, ...); backslash
        return oldPathName.insert(match.capturedStart(0),"_1");
    } else { // Previous number (_2, _3, ...); backslash
        return oldPathName.replace(
                match.capturedStart(2), match.capturedLength(2), QString::number(match.captured(2).toInt()+1));
    }
}
