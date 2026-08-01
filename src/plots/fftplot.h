#ifndef FFTPLOT_H
#define FFTPLOT_H

#include <QObject>
#include <QWidget>
#include <QMap>
#include <QThread>
#include <QMutex>
#include <QPointer>
#include <QButtonGroup>

class QSettings;

namespace Ui {
    class FFTPlot;
}

namespace Plot{
    class FFTPlot;
    class FFTPlotWorker;
}

namespace App{
    enum class LoadOption; Q_DECLARE_FLAGS(LoadOptions, LoadOption)
}

class Plot::FFTPlotWorker: public QThread
{
    Q_OBJECT
public:
        void updateData(const QMap<int, QPair<QVector<double>, QVector<double>>> &newData);
        void run() override;
        void reset();
        void updateParameter(const bool singleScan);

    signals:
        void plotDataReady(const QMap<int, QPair<QVector<double>, QVector<double>>> newData);

    public:
        QMutex mutex;

    private:
        QMap<int, QVector<double>> xData;
        QMap<int, QVector<double>> yData;
        QMap<int, QVector<double>> averageData;

        bool singleScan;
        int scanCounter;
    };

class Plot::FFTPlot: public QWidget
{
    Q_OBJECT
public:
    FFTPlot(QWidget *parent=nullptr);
    virtual ~FFTPlot();

    void saveSettings(QSettings &) const {}
    void loadSettings(const QSettings &, const App::LoadOptions) {}

    void reset();

public slots:
    void updateData(const QMap<int, QPair<QVector<double>, QVector<double>>>&);
    void updatePlotData(const QMap<int, QPair<QVector<double>, QVector<double>>>&);

protected:
    virtual void changeEvent(QEvent* e) override;

private:
    void setupPlots();
    void updateNumberOfGraphs(int n);
    void updateParameter();

private slots:
    void updateYAxis();
    void updateXAxis();

private:
    QScopedPointer<Ui::FFTPlot> ui;
    QPointer<QButtonGroup> singleAverageGroup, linLogGroup;
    const int maxNumberOfChannels = 2;

    QScopedPointer<FFTPlotWorker> worker;

    bool updatePending;
};

#endif // FFTPLOT_H
