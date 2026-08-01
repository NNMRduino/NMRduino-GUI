#ifndef TIMEDOMAINPLOT_H
#define TIMEDOMAINPLOT_H

#include <QObject>
#include <QWidget>
#include <QThread>
#include <QScopedPointer>
#include <QVector>
#include <QMap>
#include <QSettings>
#include <QMutex>
#include <QQueue>
#include <QPointer>

#include "spoiler.h"

namespace Ui {
    class TimeDomainPlot;
}

namespace Plot{
    class TimeDomainPlot;
    class TimeDomainPlotWorker;
}

namespace App{
    enum class LoadOption; Q_DECLARE_FLAGS(LoadOptions, LoadOption)
}

namespace Filter{
    enum class FilterType;
    enum class WindowType;
}

class Plot::TimeDomainPlotWorker: public QThread
{
    Q_OBJECT
public:
    void run() override;
    void reset();

    double getMean(const int channel);
    void updateData(const QMap<int, QPair<QVector<double>, QVector<double>>> &newData);
    void updateParameter(const std::string newFilterType, const double newF1, const double newF2, const std::string newWindowType,
                      const int newWindowLength, const bool singleScan);

signals:
    void plotDataReady(const QMap<int, QPair<QVector<double>, QVector<double>>> newData);

private:
    QMutex mutex;

    QMap<int, QVector<double>> xData, yData, averageData;
    QMap<int, double> channelMeans;

    int scanCounter;
    double f1, f2;
    std::string windowType, filterType;
    int windowLength;
    bool singleScan;
};

class Plot::TimeDomainPlot: public QWidget
{
    Q_OBJECT
public:
    explicit TimeDomainPlot(QWidget *parent=nullptr);
    virtual ~TimeDomainPlot();

    void saveSettings(QSettings &settings) const;
    void loadSettings(const QSettings &settings, const App::LoadOptions loadOptions);

    void reset();

public slots:
    void updateData(const QMap<int, QPair<QVector<double>, QVector<double>>>&);

protected:
    virtual void changeEvent(QEvent *e) override;

private:
    void setupPlots();
    void updateParameter();
    void updateNumberOfGraphs(int n);

private slots:
    void updatePlotData(const QMap<int, QPair<QVector<double>, QVector<double>>>&);
    void updateYAxis();
    void updateXAxis();

private:
    QScopedPointer<Ui::TimeDomainPlot> ui;
    const int maxNumberOfChannels = 2;

    //QMap<int, QColor> channelColors;
    QPointer<TimeDomainPlotWorker> worker;

    bool updatePending;
};

#endif // TIMEDOMAINPLOT_H
