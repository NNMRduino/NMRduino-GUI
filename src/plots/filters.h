#ifndef FILTERS_H
#define FILTERS_H

#include <vector>
#include <map>
#include <string>
#include <QObject>

namespace Filter{
    enum class FilterType {None, LowPass, HighPass, BandPass, BandStop};
    enum class WindowType {None, Rectangular, Bartlett, Hanning, Hamming, Blackman};

    void applyFilter(QVector<double> &data, int windowLength, double f1, double f2, double sampleRate,
                     std::string filterType, std::string windowType);

    extern const std::map<std::string, FilterType> filterTypes;
    extern const std::map<std::string, WindowType> windowTypes;
}

#endif // FILTERS_H
