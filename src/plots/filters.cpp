#include "filters.h"
#include <cstdio>

// FIR filters by Windowing
// A.Greensted - Feb 2010
// http://www.labbookpages.co.uk

#include <math.h>

#include <QDebug>

using namespace Filter;

/*int main(void)
{
    int windowLength = 201;
    double sampFreq = 44100;

    // Low and high pass filters
    double transFreq = 10000;

    double *lpf = create1TransSinc(windowLength, transFreq, sampFreq, LOW_PASS);
    double *lpf_hamming = createWindow(lpf, NULL, windowLength, HAMMING);
    double *lpf_blackman = createWindow(lpf, NULL, windowLength, BLACKMAN);

    double *hpf = create1TransSinc(windowLength, transFreq, sampFreq, HIGH_PASS);
    double *hpf_hamming = createWindow(hpf, NULL, windowLength, HAMMING);

    outputFFT("lpf-hamming.dat", lpf_hamming, windowLength, sampFreq);
    outputFFT("lpf-blackman.dat", lpf_blackman, windowLength, sampFreq);
    outputFFT("hpf-hamming.dat", hpf_hamming, windowLength, sampFreq);

    // Band pass and band stop filters
    double trans1Freq = 5000;
    double trans2Freq = 17050;

    double *bpf = create2TransSinc(windowLength, trans1Freq, trans2Freq, sampFreq, BAND_PASS);
    double *bpf_hamming = createWindow(bpf, NULL, windowLength, HAMMING);

    double *bsf = create2TransSinc(windowLength, trans1Freq, trans2Freq, sampFreq, BAND_STOP);
    double *bsf_hamming = createWindow(bsf, NULL, windowLength, HAMMING);

    outputFFT("bpf-hamming.dat", bpf_hamming, windowLength, sampFreq);
    outputFFT("bsf-hamming.dat", bsf_hamming, windowLength, sampFreq);

    // Kaiser Window
    int kaiserWindowLength;
    double beta;

    calculateKaiserParams(0.001, 800, sampFreq, &kaiserWindowLength, &beta);

    lpf = create1TransSinc(kaiserWindowLength, transFreq, sampFreq, LOW_PASS);
    double *lpf_kaiser = createKaiserWindow(lpf, NULL, kaiserWindowLength, beta);

    outputFFT("lpf-kaiser.dat", lpf_kaiser, kaiserWindowLength, sampFreq);

    return 0;
}*/

// Create sinc function for filter with 1 transition - Low and High pass filters
std::vector<double> create1TransSinc(int windowLength, double transFreq, double sampFreq, FilterType type)
{
    if (windowLength < 1){
        windowLength = 1;
    }

    // Allocate memory for the window
    std::vector<double> window(windowLength);
    //std::fill(window.begin(), window.end(), 0.0);
    window[0] = 1.0;

    if (type != FilterType::LowPass && type != FilterType::HighPass) {
        std::fprintf(stderr, "create1TransSinc: Bad filter type, should be either LOW_PASS or HIGH_PASS\n");
        return window;
    }

    if (sampFreq <= 0) {
        std::fprintf(stderr, "create1TransSinc: Sampling rate should be > 0\n");
        return window;
    }

    // Calculate the normalised transistion frequency. As transFreq should be
    // less than or equal to sampFreq / 2, ft should be less than 0.5
    double ft = transFreq / sampFreq;

    if ((ft <= 0.0) or (ft > 0.5)){
        std::fprintf(stderr, "create1TransSinc: Bad transition frequency, should be > 0 and <= sample rate\n");
        return window;
    }

    double m_2 = 0.5 * (windowLength-1);
    int halfLength = windowLength / 2;

    // Set centre tap, if present
    // This avoids a divide by zero
    if (2*halfLength != windowLength) {
        double val = 2.0 * ft;

        // If we want a high pass filter, subtract sinc function from a dirac pulse
        if (type == FilterType::HighPass) val = 1.0 - val;

        window[halfLength] = val;
    }
    else if (type == FilterType::HighPass) {
        fprintf(stderr, "create1TransSinc: For high pass filter, window length must be odd\n");
        return window;
    }

    // This has the effect of inverting all weight values
    if (type == FilterType::HighPass) ft = -ft;

    // Calculate taps
    // Due to symmetry, only need to calculate half the window
    int n;
    for (n=0 ; n<halfLength ; n++) {
        double val = sin(2.0 * M_PI * ft * (n-m_2)) / (M_PI * (n-m_2));

        window[n] = val;
        window[windowLength-n-1] = val;
    }

    //qDebug() << window;
    return window;
}

// Create two sinc functions for filter with 2 transitions - Band pass and band stop filters
std::vector<double> create2TransSinc(int windowLength, double trans1Freq, double trans2Freq, double sampFreq,
                                     FilterType type)
{
    if (windowLength < 1){
        windowLength = 1;
    }

    // Allocate memory for the window
    std::vector<double> window(windowLength);

    if (type != FilterType::BandPass && type != FilterType::BandStop) {
        fprintf(stderr, "create2TransSinc: Bad filter type, should be either BAND_PASS or BAND_STOP\n");
        return window;
    }

    if (sampFreq <= 0) {
        std::fprintf(stderr, "create2TransSinc: Sampling rate should be > 0\n");
        return window;
    }

    if (trans1Freq >= trans2Freq) {
        std::fprintf(stderr, "create2TransSinc: Bad transitions frequencies, trans1Freq <(!) trans2Freq\n");
        return window;
    }

    if (trans1Freq < 0.0){
        std::fprintf(stderr, "create2TransSinc: Bad transition frequency, trans1Freq >=(!) 0\n");
        return window;
    }

    if (trans2Freq > 0.5*sampFreq){
        std::fprintf(stderr, "create2TransSinc: Bad transition frequency, trans2Freq <=(!) sampFreq/2\n");
        return window;
    }

    // Calculate the normalised transistion frequencies.
    double ft1 = trans1Freq / sampFreq;
    double ft2 = trans2Freq / sampFreq;

    double m_2 = 0.5 * (windowLength-1);
    int halfLength = windowLength / 2;

    // Set centre tap, if present
    // This avoids a divide by zero
    if (2*halfLength != windowLength) {
        double val = 2.0 * (ft2 - ft1);

        // If we want a band stop filter, subtract sinc functions from a dirac pulse
        if (type == FilterType::BandStop){
            val = 1.0 - val;
        }

        window[halfLength] = val;
    }
    else {
        fprintf(stderr, "create1TransSinc: For band pass and band stop filters, window length must be odd\n");
        return window;
    }

    // Swap transition points if Band Stop
    if (type == FilterType::BandStop) {
        double tmp = ft1;
        ft1 = ft2; ft2 = tmp;
    }

    // Calculate taps
    // Due to symmetry, only need to calculate half the window
    int n;
    for (n=0 ; n<halfLength ; n++) {
        double val1 = sin(2.0 * M_PI * ft1 * (n-m_2)) / (M_PI * (n-m_2));
        double val2 = sin(2.0 * M_PI * ft2 * (n-m_2)) / (M_PI * (n-m_2));

        window[n] = val2 - val1;
        window[windowLength-n-1] = val2 - val1;
    }

    return window;
}

// Create a set of window weights
// in - If not null, each value will be multiplied with the window weight
// out - The output weight values, if NULL and new array will be allocated
// windowLength - the number of weights
// windowType - The window type
std::vector<double> createWindow(std::vector<double> in, int windowLength, WindowType type){
    if (windowLength < 1){
        windowLength = 1;
    }

    std::vector<double> out(windowLength);

    int n;
    int m = windowLength - 1;
    int halfLength = windowLength / 2;

    // Calculate taps
    // Due to symmetry, only need to calculate half the window
    switch (type)
    {
        case WindowType::None:
            return out;

        case WindowType::Rectangular:
            for (n=0 ; n<windowLength ; n++) {
                out[n] = 1.0;
            }
            break;

        case WindowType::Bartlett:
            for (n=0 ; n<=halfLength ; n++) {
                double tmp = (double) n - (double)m / 2;
                double val = 1.0 - (2.0 * fabs(tmp))/m;
                out[n] = val;
                out[windowLength-n-1] = val;
            }

            break;

        case WindowType::Hanning:
            for (n=0 ; n<=halfLength ; n++) {
                double val = 0.5 - 0.5 * cos(2.0 * M_PI * n / m);
                out[n] = val;
                out[windowLength-n-1] = val;
            }

            break;

        case WindowType::Hamming:
            for (n=0 ; n<=halfLength ; n++) {
                double val = 0.54 - 0.46 * cos(2.0 * M_PI * n / m);
                out[n] = val;
                out[windowLength-n-1] = val;
            }
            break;

        case WindowType::Blackman:
            for (n=0 ; n<=halfLength ; n++) {
                double val = 0.42 - 0.5 * cos(2.0 * M_PI * n / m) + 0.08 * cos(4.0 * M_PI * n / m);
                out[n] = val;
                out[windowLength-n-1] = val;
            }
            break;
    }

    // If input has been given, multiply with out
    if (in.size() == out.size()) {
        for (n=0 ; n<windowLength ; n++) {
            out[n] *= in[n];
        }
    }

    return out;
}

std::list<std::string> getFilterTypes(){
    std::list<std::string> filterTypesList;

    for (const auto &myPair : filterTypes ) {
        filterTypesList.push_back(myPair.first);
    }

    return filterTypesList;
}

std::list<std::string> getWindowTypes(){
    std::list<std::string> windowTypeList;

    for (const auto &myPair : windowTypes ) {
        windowTypeList.push_back(myPair.first);
    }

    return windowTypeList;
}

std::vector<double> createWindow(int windowLength, double trans1Freq, double trans2Freq, double sampFreq,
                                 FilterType filterType, WindowType windowType)
{
    std::vector<double> transSinc;

    switch(filterType){
    case FilterType::None:
        return std::vector<double>();
    case FilterType::LowPass:
    case FilterType::HighPass:
        transSinc = create1TransSinc(windowLength, trans1Freq, sampFreq, filterType);
        break;
    case FilterType::BandPass:
    case FilterType::BandStop:
        transSinc = create2TransSinc(windowLength, trans1Freq, trans2Freq, sampFreq, filterType);
        break;
    }

    switch(windowType){
    case WindowType::None:
        return std::vector<double>(windowLength);
    case WindowType::Hamming:
    case WindowType::Hanning:
    case WindowType::Rectangular:
    case WindowType::Blackman:
    case WindowType::Bartlett:
        return createWindow(transSinc, windowLength, windowType);
    }

    return std::vector<double>(windowLength);
}


std::vector<double> createWindow(int windowLength, double transFreq, double sampFreq, FilterType filterType,
                                 WindowType windowType)
{
    switch(filterType){
    case FilterType::LowPass:
    case FilterType::HighPass:
        return createWindow(windowLength, transFreq, 0.0, sampFreq, filterType, windowType);
    default:
        return std::vector<double>();
    }
}


const std::map<std::string, FilterType> Filter::filterTypes(
        {{"None",           FilterType::None},
         {"LowPass",        FilterType::LowPass},
         {"HighPass",       FilterType::HighPass},
         {"BandPass",       FilterType::BandPass},
         {"BandStop",       FilterType::BandStop}});

const std::map<std::string, WindowType> Filter::windowTypes(
        {{"None",           WindowType::None},
         {"Rectangular",    WindowType::Rectangular},
         {"Bartlett",       WindowType::Bartlett},
         {"Hanning",        WindowType::Hanning},
         {"Hamming",        WindowType::Hamming},
         {"Blackman",       WindowType::Blackman},});



namespace Filter{
    void applyFilter(QVector<double> &data, int windowLength, double f1, double f2, double sampleRate,
                 FilterType filterType, WindowType windowType);
}

void Filter::applyFilter(QVector<double> &data, int windowLength, double f1, double f2, double sampleRate,
                 FilterType filterType, WindowType windowType)
{
    auto unfilteredData = data;

    if ((filterType == FilterType::None) or (windowType == WindowType::None)){
        return;
    }

    auto filter = createWindow(windowLength, f1, f2, sampleRate, filterType, windowType);
    windowLength = filter.size();

    if (windowLength > unfilteredData.size()){
        fprintf(stderr, "applyFilter: Bad window length, windowLength <=(!) dataLength\n");
        return;
    }

    //for(int i = 0; i < unfilteredData.size(); i++){
    //    data[i] = 0.0;
    //    for (unsigned long long j=0; (j < filter.size()) and (j <= static_cast<unsigned int>(i)); j++){
    //        data[i] += unfilteredData[i-j] * filter[j];
    //    }
    //}

    //for(int i = 0; i < unfilteredData.size(); i++){
    //    data[i] = 0.0;
    //    for(int j=std::max(windowLength/2-i, 0); j<std::min(unfilteredData.size()-i+windowLength/2, windowLength); j++){
    //        data[i] += unfilteredData[i - windowLength/2 + j] * filter[j];
    //    }
    //}

    data.resize(unfilteredData.size() - windowLength - 1);
    for(int i = 0; i < data.size(); i++){
        data[i] = 0.0;
        for(int j=0; j<windowLength; j++){
            data[i] += unfilteredData[i+j] * filter[j];
        }
    }

    //qDebug() << filter.size();
}



void Filter::applyFilter(QVector<double> &data, int windowLength, double f1, double f2, double sampleRate,
                                               std::string filterTypeStr, std::string windowTypeStr)
{
    Filter::FilterType filterType;
    if (filterTypes.count(filterTypeStr)){
        filterType = filterTypes.at(filterTypeStr);
    } else {
        filterType = FilterType::None;
    }

    Filter::WindowType windowType;
    if (windowTypes.count(windowTypeStr)){
        windowType = windowTypes.at(windowTypeStr);
    } else {
        windowType = WindowType::None;
    }

    applyFilter(data, windowLength, f1, f2, sampleRate, filterType, windowType);
}


