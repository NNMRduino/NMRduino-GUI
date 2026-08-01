#include "timedomainplot.h"
#include "ui_timedomainplot.h"

#include <QDebug>
#include <string>
#include <QPalette>

#include "filters.h"
#include "mainapplication.h"
#include <algorithm>

#include "spoiler.h"

using namespace Plot;

TimeDomainPlot::TimeDomainPlot(QWidget *parent):
    QWidget(parent),
    ui(new Ui::TimeDomainPlot),
    worker(new TimeDomainPlotWorker)
{
    ui->setupUi(this);

    qRegisterMetaType<QMap<int, QPair<QVector<double>, QVector<double>>>>();

    //auto anyLayout = new QGridLayout();
    //anyLayout->addWidget(ui->toolBox);

    auto spoiler = new Spoiler("", 300, this);
    spoiler->addWidget(ui->toolBox);
    ui->gridLayout->addWidget(spoiler, 0, 1);

    ui->comboBox_filterType->clear();
    for(auto const &filterTypeStr: Filter::filterTypes)
        ui->comboBox_filterType->addItem(QString::fromStdString(filterTypeStr.first));

    ui->comboBox_windowType->clear();
    for(auto const &windowTypeStr: Filter::windowTypes)
        ui->comboBox_windowType->addItem(QString::fromStdString(windowTypeStr.first));
    ui->comboBox_filterType->setCurrentText("None");

    connect(worker, &TimeDomainPlotWorker::plotDataReady, this, &TimeDomainPlot::updatePlotData);

    setupPlots();

    connect(ui->doubleSpinBox_offset, &QDoubleSpinBox::editingFinished, this, [=] () {updateYAxis(); });
    connect(ui->doubleSpinBox_zoom, &QDoubleSpinBox::editingFinished, this, [=] () {updateYAxis();});
    connect(ui->checkBox_saSignalPlotManualOffset, &QCheckBox::stateChanged, this,
            [=] () {ui->doubleSpinBox_offset->setEnabled(ui->checkBox_saSignalPlotManualOffset->isChecked());
                    updateYAxis();});

    connect(ui->doubleSpinBox_begin, &QDoubleSpinBox::editingFinished, this, [=] () {updateXAxis();});
    connect(ui->doubleSpinBox_end, &QDoubleSpinBox::editingFinished, this, [=] () {updateXAxis();});

    connect(ui->comboBox_filterType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=] () {updateParameter();});
    connect(ui->comboBox_windowType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=] () {updateParameter();});

    connect(ui->doubleSpinBox_f1, &QDoubleSpinBox::editingFinished,
            this, &TimeDomainPlot::updateParameter);
    connect(ui->doubleSpinBox_f2, &QDoubleSpinBox::editingFinished,
            this, &TimeDomainPlot::updateParameter);

    connect(ui->spinBox_windowLength, &QSpinBox::editingFinished,
            this, &TimeDomainPlot::updateParameter);

    connect(ui->radioButton_singleScan, &QRadioButton::clicked,
            this, &TimeDomainPlot::updateParameter);
    connect(ui->radioButton_averaged, &QRadioButton::clicked,
            this, &TimeDomainPlot::updateParameter);

    updateParameter();

    connect(ui->qcustomplot_plot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
            this, [=](const QCPRange &newRange)
            {
                if (newRange.lower < 0){ui->qcustomplot_plot->xAxis->setRange(0, newRange.size()); return;}
                ui->doubleSpinBox_begin->setValue(newRange.lower*1000.0);
                ui->doubleSpinBox_end->setValue(newRange.upper*1000.0);
            });

    connect(ui->qcustomplot_plot->yAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
            this, [=](const QCPRange &newRange)
            {ui->doubleSpinBox_offset->setValue((newRange.upper + newRange.lower)/2.0);
             ui->doubleSpinBox_zoom->setValue(10.0/(newRange.upper - newRange.lower));});


    updateXAxis();
    updateYAxis();
}

TimeDomainPlot::~TimeDomainPlot(){
    if (worker){
        if (worker->isRunning()){
            worker->wait();
        }
        delete worker;
    }
}


void Plot::TimeDomainPlot::setupPlots(){

    updateNumberOfGraphs(1);

    ui->qcustomplot_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);// | QCP::iSelectAxes);
    ui->qcustomplot_plot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
    ui->qcustomplot_plot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);

    ui->qcustomplot_plot->xAxis->setLabel("Time (s)");
    ui->qcustomplot_plot->yAxis->setLabel("Signal (V)");

    ui->qcustomplot_plot->xAxis2->setVisible(true);
    ui->qcustomplot_plot->yAxis2->setVisible(true);
    ui->qcustomplot_plot->xAxis2->setTicks(false);
    ui->qcustomplot_plot->yAxis2->setTicks(false);
}

void TimeDomainPlot::updateParameter(){
    if (not worker){
        qCritical() << "No worker thread found!";
        return;
    }

    worker->updateParameter(
            ui->comboBox_filterType->currentText().toStdString(),
            ui->doubleSpinBox_f1->value(),
            ui->doubleSpinBox_f2->value(),
            ui->comboBox_windowType->currentText().toStdString(),
            ui->spinBox_windowLength->value(),
            ui->radioButton_singleScan->isChecked()
                );

    if (worker->isRunning()){
        updatePending = true;
        return;
    }

    updatePending = false;
    worker->start();
}

void TimeDomainPlot::updateNumberOfGraphs(int n)
{
    n = std::min(n, maxNumberOfChannels);

    if (ui->qcustomplot_plot->graphCount() == n){
        return;
    }

    QVector<double> x(100), y(100, 1.0); // initialize with entries 0..100
    std::generate(x.begin(), x.end(), [n = 0, a = .01] () mutable {return n++ * a; });

    for (int i=0; i < n; i++){
        ui->qcustomplot_plot->addGraph(); // Mode 2 spectrum analyzer, time domain signal plot, top left
        ui->qcustomplot_plot->graph(i)->setData(x, y);
        auto plotColor = (i != 1
                          ? ui->qcustomplot_plot->getColorFromPalette(i, 1)
                          : ui->qcustomplot_plot->getColorFromPalette(i, 1, MyQCustomPlot::Palettes::Secondary));
#ifdef QCUSTOMPLOT_USE_OPENGL
        ui->qcustomplot_plot->graph(i)->setPen(QPen(plotColor, 2));
#else
        ui->qcustomplot_plot->graph(i)->setPen(QPen(plotColor, 1));
#endif
        ui->qcustomplot_plot->graph(i)->setLineStyle(QCPGraph::lsStepRight);
    }
}

void TimeDomainPlot::updatePlotData(const QMap<int, QPair<QVector<double>, QVector<double> > > & data){
    const auto keys = data.keys();

    auto max_channel = *std::max_element(std::begin(keys), std::end(keys));
    updateNumberOfGraphs(max_channel + 1);

    const auto channels = data.keys();
    for(auto channel: channels){
        if ((channel < 0) or (channel >= maxNumberOfChannels)){
            continue;
        }

        ui->qcustomplot_plot->graph(channel)->setData(data.value(channel).first, data.value(channel).second, true);
    }

    if (ui->checkBox_saSignalPlotManualOffset->isChecked()){
        ui->qcustomplot_plot->replot();
    } else {
        updateYAxis();
    }

    if (updatePending){
        updatePending = false;
        if (worker){
            worker->start();
        } else {
            qCritical() << "No worker thread found!";
        }
    }
}

void TimeDomainPlot::updateYAxis(){
    double offset = ui->doubleSpinBox_offset->value();
    double zoom = ui->doubleSpinBox_zoom->value();

    if (ui->checkBox_saSignalPlotManualOffset->isChecked()){
        ui->qcustomplot_plot->yAxis->setRange(-5.0/zoom + offset, 5.0/zoom + offset);
    } else {
        double mean = 0;
        if (worker){
            mean = worker->getMean(0);
        }

        ui->qcustomplot_plot->yAxis->setRange(-5.0/zoom + mean, 5.0/zoom + mean);
    }
    //qDebug() << ui->qcustomplot_plot->xAxis->grid()->subGridPen();
    ui->qcustomplot_plot->replot();
}

void TimeDomainPlot::updateXAxis(){
    auto begin = ui->doubleSpinBox_begin->value();
    auto end = ui->doubleSpinBox_end->value();

    ui->qcustomplot_plot->xAxis->setRange(begin/1000, end/1000);

    ui->qcustomplot_plot->replot();
}



void TimeDomainPlot::saveSettings(QSettings &settings) const{
    settings.setValue("FilterType", ui->comboBox_filterType->currentText());
    settings.setValue("WindowType", ui->comboBox_windowType->currentText());

    settings.setValue("FilterF1", ui->doubleSpinBox_f1->value());
    settings.setValue("FilterF2", ui->doubleSpinBox_f2->value());
    settings.setValue("WindowLength", ui->spinBox_windowLength->value());

    settings.setValue("SingleScan", ui->radioButton_singleScan->isChecked());
    settings.setValue("Averaged", ui->radioButton_averaged->isChecked());
}


void TimeDomainPlot::updateData(const QMap<int, QPair<QVector<double>, QVector<double> > > &newData){
    if (not worker){
        qCritical() << "No worker thread found!";
        return;
    }

    worker->updateData(newData);

    if (worker->isRunning()){
        updatePending = true;        
        return;
    }

    updatePending = false;
    worker->start();
}

void TimeDomainPlot::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::StyleChange)
    {
        for (int i=0; i < ui->qcustomplot_plot->graphCount(); ++i){
            auto plotColor = (i != 1
                                  ? ui->qcustomplot_plot->getColorFromPalette(i, 1)
                                  : ui->qcustomplot_plot->getColorFromPalette(i, 1, MyQCustomPlot::Palettes::Secondary));
#ifdef QCUSTOMPLOT_USE_OPENGL
            ui->qcustomplot_plot->graph(i)->setPen(QPen(plotColor, 2));
#else
            ui->qcustomplot_plot->graph(i)->setPen(QPen(plotColor, 1));
#endif
        }
        ui->qcustomplot_plot->replot();
    }

    QWidget::changeEvent(e);
}

void TimeDomainPlot::loadSettings(const QSettings &settings, const App::LoadOptions loadOptions){
    if(loadOptions & App::LoadOption::LoadPlotSettings){
        ui->comboBox_filterType->setCurrentText(settings.value("FilterType").toString());
        ui->comboBox_windowType->setCurrentText(settings.value("WindowType").toString());

        ui->doubleSpinBox_f1->setValue(settings.value("FilterF1").toDouble());
        ui->doubleSpinBox_f2->setValue(settings.value("FilterF2").toDouble());
        ui->spinBox_windowLength->setValue(settings.value("WindowLength").toDouble());

        ui->radioButton_singleScan->setChecked(settings.value("SingleScan").toBool());
        ui->radioButton_averaged->setChecked(settings.value("Averaged").toBool());

        updateParameter();
    }
}

void TimeDomainPlot::reset()
{
    if (not worker){
        qCritical() << "No worker thread found!";
        return;
    }
    worker->reset();
}

void TimeDomainPlotWorker::updateData(const QMap<int, QPair<QVector<double>, QVector<double> > > &newData)
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

void TimeDomainPlotWorker::updateParameter(std::string newFilterType, double newF1, double newF2,
                                        std::string newWindowType, const int newWindowLength,
                                        const bool newSingleScan)
{
    QMutexLocker locker(&mutex);

    filterType = newFilterType;
    windowType = newWindowType;

    f1 = newF1;
    f2 = newF2;

    windowLength = newWindowLength;

    singleScan = newSingleScan;
}

void TimeDomainPlotWorker::run(){
    // Create copy of relevant data
    QMutexLocker locker(&mutex);
    auto f1Copy = f1, f2Copy = f2;

    auto windowTypeCopy = windowType;
    auto filterTypeCopy = filterType;

    auto windowLengthCopy = windowLength;

    auto scanCounterCopy = scanCounter;

    auto xDataCopy = xData;

    QMap<int, QVector<double>> yDataCopy;
    auto singleScanCopy = singleScan;
    if (singleScanCopy){
        yDataCopy = yData;
    } else {
        yDataCopy = averageData;
    }

    locker.unlock();

    double sampleRate, mean;
    QVector<double> *xDataPtr, *yDataPtr;

    QMap<int, QPair<QVector<double>, QVector<double>>> result;

    auto channelList = xDataCopy.keys();
    for (auto channel: qAsConst(channelList)){

        xDataPtr = &xDataCopy[channel];
        yDataPtr = &yDataCopy[channel];

        if (not singleScanCopy){
            std::transform(yDataPtr->begin(), yDataPtr->end(), yDataPtr->begin(),
                           std::bind(std::multiplies<double>(), std::placeholders::_1, 1.0/scanCounterCopy));
        }

        // calculate sample rate
        if(xDataPtr->size() >= 2){
            sampleRate = 1/((*xDataPtr)[1] - (*xDataPtr)[0]);
        } else {
            continue;
        }

        // apply filter
        Filter::applyFilter(*yDataPtr, windowLengthCopy, f1Copy, f2Copy, sampleRate, filterTypeCopy, windowTypeCopy);
        xDataPtr->resize(yDataPtr->size());

        // calculate mean
        mean = std::accumulate(yDataPtr->begin(), yDataPtr->end(), .0) / yDataPtr->size();
        channelMeans.insert(channel, mean);

        result.insert(channel, {*xDataPtr, *yDataPtr});
    }

    emit plotDataReady(result);
}

void TimeDomainPlotWorker::reset()
{
    QMutexLocker lockerInput(&mutex);
    scanCounter = 0;
    averageData.clear();
}

double TimeDomainPlotWorker::getMean(const int channel){
    QMutexLocker locker(&mutex);
    return channelMeans.value(channel, 0.0);
}
