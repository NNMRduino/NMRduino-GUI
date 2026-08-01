#ifndef FFT_H
#define FFT_H

#include <QVector>

void calculateFft(QVector<double> &amplData, const QVector<double> &voltageData, const double sampleRate);

void calculateFftFrequency(QVector<double> &freqData, const int numberOfSamples, const double sampleRate);

#endif // FFT_H
