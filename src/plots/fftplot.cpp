#include "fftplot.h"
#include "ui_fftplot.h"

#include <algorithm>

#include "fft.h"
#include "spoiler.h"

using namespace Plot;

void FFTPlotWorker::run()
{
    QMutexLocker locker(&mutex);
    auto xDataCopy = xData;

    auto scanCounterCopy = scanCounter;
    auto singleScanCopy = singleScan;
    QMap<int, QVector<double>> yDataCopy;
    if (singleScanCopy){
        yDataCopy = yData;
    } else {
        yDataCopy = averageData;
    }

    locker.unlock();

    QMap<int, QPair<QVector<double>, QVector<double>>> result;
    double samplingRate;
    QVector<double> *xDataPtr, *yDataPtr;

    const auto keys = xDataCopy.keys();
    for (auto channel: keys){
        xDataPtr = &xDataCopy[channel];
        yDataPtr = &yDataCopy[channel];

        if (not singleScanCopy){
            std::transform(yDataPtr->begin(), yDataPtr->end(), yDataPtr->begin(),
                           std::bind(std::multiplies<double>(), std::placeholders::_1, 1.0/scanCounterCopy));
        }

        if(xDataPtr->size() >= 2){
            samplingRate = 1/((*xDataPtr)[1] - (*xDataPtr)[0]);
        } else {
            continue;
        }

        QVector<double> freqData;
        calculateFftFrequency(freqData, xDataPtr->size(), samplingRate);

        QVector<double> amplData;
        calculateFft(amplData, *yDataPtr, samplingRate);

        result.insert(channel, {freqData, amplData});
    }
    emit plotDataReady(result);
}

void FFTPlotWorker::reset()
{
    QMutexLocker lockerInput(&mutex);
    scanCounter = 0;
    averageData.clear();
}

void FFTPlotWorker::updateParameter(const bool newSingleScan)
{
    QMutexLocker locker(&mutex);
    singleScan = newSingleScan;
}

void FFTPlotWorker::updateData(const QMap<int, QPair<QVector<double>, QVector<double>>> &newData)
{
    QMutexLocker locker(&mutex);

    const QVector<double> *yDataPtr;
    QVector<double> *yAveragePtr;

    const auto channelList = newData.keys();
    for(auto channel: channelList){
        xData.insert(channel, newData.value(channel).first);
        yData.insert(channel, newData.value(channel).second);

        yDataPtr = &yData[channel];
        yAveragePtr = &averageData[channel];

        if (yAveragePtr->size() != yDataPtr->size()){
            averageData.insert(channel, *yDataPtr);
            scanCounter = 1;
        } else {
            std::transform(yAveragePtr->begin(), yAveragePtr->end(), yDataPtr->begin(), yAveragePtr->begin(), std::plus<double>());
            scanCounter++;
        }
    }
}

FFTPlot::FFTPlot(QWidget *parent):
    QWidget(parent),
    ui(new Ui::FFTPlot),    
    singleAverageGroup(new QButtonGroup(parent)),
    linLogGroup(new QButtonGroup(parent)),
    worker(new FFTPlotWorker)
{
    ui->setupUi(this);

    //auto anyLayout = new QGridLayout();
    //anyLayout->addWidget(ui->toolBox);

    auto spoiler = new Spoiler("", 300, this);
    spoiler->addWidget(ui->toolBox);
    ui->gridLayout->addWidget(spoiler, 0, 1);

    singleAverageGroup->addButton(ui->radioButton_singleScan);
    singleAverageGroup->addButton(ui->radioButton_average);

    linLogGroup->addButton(ui->radioButton_linScale);
    linLogGroup->addButton(ui->radioButton_logScale);

    ui->radioButton_singleScan->setChecked(true);
    ui->radioButton_linScale->setChecked(true);

    connect(ui->doubleSpinBox_start, &QDoubleSpinBox::editingFinished, this, [=] () {updateXAxis();});
    connect(ui->doubleSpinBox_end, &QDoubleSpinBox::editingFinished, this, [=] () {updateXAxis();});
    connect(ui->doubleSpinBox_min, &QDoubleSpinBox::editingFinished, this, [=] () {updateYAxis();});
    connect(ui->doubleSpinBox_max, &QDoubleSpinBox::editingFinished, this, [=] () {updateYAxis();});

    connect(ui->plot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
            this, [=](const QCPRange &newRange)
            {if (newRange.lower < 0){ui->plot->xAxis->setRange(0, newRange.size()); return;}
             ui->doubleSpinBox_start->setValue(newRange.lower); ui->doubleSpinBox_end->setValue(newRange.upper);});

    connect(ui->plot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
            this, [=](const QCPRange &newRange)
            {if (newRange.lower < 0){ui->plot->yAxis->setRange(0, newRange.size()); return;}
             ui->doubleSpinBox_min->setValue(newRange.lower); ui->doubleSpinBox_max->setValue(newRange.upper);});

    connect(ui->radioButton_linScale, &QRadioButton::toggled, this, [=] () {updateYAxis();});
    connect(ui->radioButton_logScale, &QRadioButton::toggled, this, [=] () {updateYAxis();});

    connect(ui->radioButton_singleScan, &QRadioButton::clicked,
            this, &FFTPlot::updateParameter);
    connect(ui->radioButton_average, &QRadioButton::clicked,
            this, &FFTPlot::updateParameter);

    updateParameter();


    connect(worker.data(), &FFTPlotWorker::plotDataReady, this, &FFTPlot::updatePlotData);

    setupPlots();

    updateXAxis();
    updateYAxis();
}

FFTPlot::~FFTPlot()
{
    if (worker->isRunning()){
        worker->wait();
    }
}

void FFTPlot::updateData(const QMap<int, QPair<QVector<double>, QVector<double> > > &newData)
{
    worker->updateData(newData);
    if (worker->isRunning()){
        updatePending = true;
        return;
    } else {
        updatePending = false;
        worker->start();
        return;
    }
}
void FFTPlot::setupPlots()
{
    updateNumberOfGraphs(1);

    ui->plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);// | QCP::iSelectAxes);
    ui->plot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    ui->plot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);

    ui->plot->xAxis->setLabel("Frequency (Hz)");
    ui->plot->yAxis->setLabel("Amplitude (mV/√Hz)");
    ui->plot->xAxis->setRange(0, 600);
    ui->plot->yAxis->setRange(0, 900);
    ui->plot->xAxis2->setVisible(true);
    ui->plot->yAxis2->setVisible(true);
    ui->plot->xAxis2->setTicks(false);
    ui->plot->yAxis2->setTicks(false);
    ui->plot->replot();
}

void FFTPlot::updateNumberOfGraphs(int n)
{
    n = std::min(n, maxNumberOfChannels);

    if (ui->plot->graphCount() == n){
        return;
    }

    QVector<double> x(10), y(10, 0.0); // initialize with entries 0..100
    //std::generate(x.begin(), x.end(), [n = 0, a = .01]() mutable { return n++ * a; });

    ui->plot->clearGraphs();
    for (int i=0; i < n; ++i){
        ui->plot->addGraph(); // Mode 2 spectrum analyzer, lowest vertical zoom
        ui->plot->graph(i)->setData(x, y);
        auto plotColor = (i != 1
                              ? ui->plot->getColorFromPalette(i, 1)
                              : ui->plot->getColorFromPalette(i, 1, MyQCustomPlot::Palettes::Secondary));
#ifdef QCUSTOMPLOT_USE_OPENGL
        ui->plot->graph(i)->setPen(QPen(plotColor, 2));
#else
        ui->plot->graph(i)->setPen(QPen(plotColor, 1));
#endif
        ui->plot->graph(i)->setLineStyle(QCPGraph::lsLine);
    }
}

void FFTPlot::updateParameter()
{
    if (not worker){
        qCritical() << "No worker thread found!";
        return;
    }

    worker->updateParameter(ui->radioButton_singleScan->isChecked());

    if (worker->isRunning()){
        updatePending = true;
        return;
    }

    updatePending = false;
    worker->start();
}

void FFTPlot::updatePlotData(const QMap<int, QPair<QVector<double>, QVector<double> > > &data)
{
    const auto keys = data.keys();

    auto max_channel = *std::max_element(std::begin(keys), std::end(keys));
    updateNumberOfGraphs(max_channel + 1);

    for(auto channel: keys){
        if ((channel < 0) or (channel >= maxNumberOfChannels)){
            continue;
        }        

        ui->plot->graph(channel)->setData(data.value(channel).first, data.value(channel).second, true);
    }

    ui->plot->replot();

    if (updatePending){
        updatePending = false;
        worker->start();
    }
}

void FFTPlot::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::StyleChange)
    {
        for (int i=0; i < ui->plot->graphCount(); ++i){
            auto plotColor = (i != 1
                                  ? ui->plot->getColorFromPalette(i, 1)
                                  : ui->plot->getColorFromPalette(i, 1, MyQCustomPlot::Palettes::Secondary));
#ifdef QCUSTOMPLOT_USE_OPENGL
            ui->plot->graph(i)->setPen(QPen(plotColor, 2));
#else
            ui->plot->graph(i)->setPen(QPen(plotColor, 1));
#endif
        }
        ui->plot->replot();
    }

    QWidget::changeEvent(e);
}

void FFTPlot::updateYAxis()
{
    if (ui->radioButton_linScale->isChecked()) {
        ui->plot->yAxis->setScaleType(QCPAxis::stLinear);
        ui->plot->yAxis->setTicker(QSharedPointer<QCPAxisTicker>::create());
    }
    else {
        ui->plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
        ui->plot->yAxis->setTicker(QSharedPointer<QCPAxisTickerLog>::create());
    }

    ui->plot->yAxis->setRange(ui->doubleSpinBox_min->value(), ui->doubleSpinBox_max->value());

    ui->plot->replot();
}

void FFTPlot::updateXAxis()
{
    ui->plot->xAxis->setRange(ui->doubleSpinBox_start->value(), ui->doubleSpinBox_end->value());
    ui->plot->replot();
}

void FFTPlot::reset()
{
    if (not worker){
        qCritical() << "No worker thread found!";
        return;
    }
    worker->reset();
}
