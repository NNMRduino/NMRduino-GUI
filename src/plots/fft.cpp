#include "fft.h"

#include <complex>

void calculateFft(QVector<double> &amplData, const QVector<double> &voltageData, const double sampleRate){
    int N = voltageData.size();

    if (N < 2){
        std::fprintf(stderr, "calculateFft: Too few samples (N < 2)\n");
        return;
    }

    QVector<std::complex<double>> dataComplex(N);
    for(int i=0; i<N; i++){
        dataComplex[i] = {static_cast<double>(voltageData[i]), 0.0};
    }

    // Obtain the spectrum with FFT radix 2
    int n;
    double thetaT = 3.14159265358979323846264338328 / N;
    std::complex<double> phiT = std::complex<double>(cos(thetaT), sin(thetaT)), T;

    int k = N;
    std::complex<double> temp;
    while (k > 1)
    {
        n = k;
        k >>= 1;
        phiT = phiT * phiT;
        T = 1.0L;
        for (int l = 0; l < k; l++)
        {
            for (int a = l; a < N; a += n)
            {
                int b = a + k;
                temp = dataComplex[a] - dataComplex[b];
                dataComplex[a] += dataComplex[b];
                dataComplex[b] = temp * T;
            }
            T *= phiT;
        }
    }

    // Decimate
    unsigned int m = static_cast<unsigned int> (log2(N));
    unsigned int b;
    for (unsigned int a = 0; a < static_cast<unsigned int>(N); a++)
    {
        b = a;
        // Reverse bits
        b = (((b & 0xaaaaaaaa) >> 1) | ((b & 0x55555555) << 1));
        b = (((b & 0xcccccccc) >> 2) | ((b & 0x33333333) << 2));
        b = (((b & 0xf0f0f0f0) >> 4) | ((b & 0x0f0f0f0f) << 4));
        b = (((b & 0xff00ff00) >> 8) | ((b & 0x00ff00ff) << 8));
        b = ((b >> 16) | (b << 16)) >> (32 - m);
        if (b > a)
        {
            temp = dataComplex[static_cast<int>(a)];
            dataComplex[static_cast<int>(a)] = dataComplex[static_cast<int>(b)];
            dataComplex[static_cast<int>(b)] = temp;
        }
    }

    if (N%2 == 0){
        amplData.resize(N/2 + 1); // QVector real and abs
    } else {
        amplData.resize((N+1)/2); // QVector real and abs
    }

    for(int i=0; i<amplData.size(); i++)    {
      amplData[i] = 1000.0 / std::sqrt(sampleRate*N) * std::abs(dataComplex[i]); // 1V -> 1000 mV
      // abs of spectrum in mV/sqrt(Hz)

      amplData[i] *= std::sqrt(2); // only positive frequencies -> most values have to be doubled ...
    }

    amplData.front() /= std::sqrt(2); // ... except first and last

    if (N%2 == 0){
        amplData.back() /= std::sqrt(2);
    }
}

void calculateFftFrequency(QVector<double> &freqData, const int numberOfSamples, const double sampleRate){
    if (numberOfSamples%2 == 0){
        freqData.resize(numberOfSamples/2 + 1);
    } else {
        freqData.resize((numberOfSamples+1)/2);
    }

    double df = sampleRate/numberOfSamples;

    std::generate(freqData.begin(), freqData.end(), [n=0, &df]() mutable { return n++ * df;});
}
