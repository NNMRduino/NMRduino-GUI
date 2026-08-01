
#ifndef MYQCUSTOMPLOT_H
#define MYQCUSTOMPLOT_H

#include "plots/qcustomplot.h"

namespace Plot{
    class TimeDomainPlot;
    class FFTPlot;
    class MyQCustomPlot;
}


class Plot::MyQCustomPlot: public QCustomPlot
{
    Q_OBJECT
    Q_PROPERTY(QColor backgroundColor MEMBER backgroundColor WRITE setBackgroundColor)

    Q_PROPERTY(QColor xAxisBasePenColor MEMBER xAxisBasePenColor WRITE setXAxisBasePenColor)
    Q_PROPERTY(QColor yAxisBasePenColor MEMBER yAxisBasePenColor WRITE setYAxisBasePenColor)
    Q_PROPERTY(QColor xAxis2BasePenColor MEMBER xAxis2BasePenColor WRITE setXAxis2BasePenColor)
    Q_PROPERTY(QColor yAxis2BasePenColor MEMBER yAxis2BasePenColor WRITE setYAxis2BasePenColor)
    Q_PROPERTY(QColor xAxisSubTickPenColor MEMBER xAxisSubTickPenColor WRITE setXAxisSubTickPenColor)
    Q_PROPERTY(QColor yAxisSubTickPenColor MEMBER yAxisSubTickPenColor WRITE setYAxisSubTickPenColor)
    Q_PROPERTY(QColor xAxisTickPenColor MEMBER xAxisTickPenColor WRITE setXAxisTickPenColor)
    Q_PROPERTY(QColor yAxisTickPenColor MEMBER yAxisTickPenColor WRITE setYAxisTickPenColor)
    Q_PROPERTY(QColor xAxisLabelColor READ getXAxisLabelColor WRITE setXAxisLabelColor)
    Q_PROPERTY(QColor yAxisLabelColor READ getYAxisLabelColor WRITE setYAxisLabelColor)
    Q_PROPERTY(QColor xAxisTickLabelColor READ getXAxisTickLabelColor WRITE setXAxisTickLabelColor)
    Q_PROPERTY(QColor yAxisTickLabelColor READ getYAxisTickLabelColor WRITE setYAxisTickLabelColor)

    Q_PROPERTY(QBrush legendBrush READ getLegendBrush WRITE setLegendBrush)
    Q_PROPERTY(QColor legendTextColor READ getLegendTextColor WRITE setLegendTextColor)

    Q_PROPERTY(QColor graph0PenColor MEMBER mainColorStyle WRITE setGraph0PenColor)
    Q_PROPERTY(QColor graph1PenColor MEMBER secondaryColorStyle WRITE setGraph1PenColor)

public:
    enum class Palettes {
        None                =  0,
        Main                =  1,
        Secondary           =  2,
    }; Q_ENUM(Palettes)

    typedef struct Palette{
        QColor mainColor;
        QList<QColor> colors;
    } Palette;

    explicit MyQCustomPlot(QWidget *parent = nullptr);
    virtual ~MyQCustomPlot() override;

    QColor getColorFromPalette(int i, int n, Palettes palette=Palettes::Main);
    QColor getColorFromPalette(int i, int n, QColor mainColor, Palettes palette=Palettes::Main);

private:
    void setBackgroundColor(QColor color);
    void setXAxisBasePenColor(QColor color);
    void setYAxisBasePenColor(QColor color);
    void setXAxis2BasePenColor(QColor color);
    void setYAxis2BasePenColor(QColor color);
    void setXAxisTickPenColor(QColor color);
    void setYAxisTickPenColor(QColor color);
    void setXAxisSubTickPenColor(QColor color);
    void setYAxisSubTickPenColor(QColor color);


    void setXAxisLabelColor(QColor color) {xAxis->setLabelColor(color);}
    void setYAxisLabelColor(QColor color) {yAxis->setLabelColor(color);}
    void setXAxisTickLabelColor(QColor color) {xAxis->setTickLabelColor(color);}
    void setYAxisTickLabelColor(QColor color) {yAxis->setTickLabelColor(color);}
    void setLegendBrush(QBrush brush) {legend->setBrush(brush);}
    void setLegendTextColor(QColor color) {legend->setTextColor(color);}

    QColor getXAxisLabelColor() {return xAxis->property("labelColor").value<QColor>();}
    QColor getYAxisLabelColor() {return yAxis->property("labelColor").value<QColor>();}
    QColor getXAxisTickLabelColor() {return xAxis->property("tickLabelColor").value<QColor>();}
    QColor getYAxisTickLabelColor() {return yAxis->property("tickLabelColor").value<QColor>();}
    QBrush getLegendBrush() {return legend->property("brush").value<QBrush>();}
    QColor getLegendTextColor() {return legend->property("textColor").value<QColor>();}

    void setGraph0PenColor(QColor color);
    void setGraph1PenColor(QColor color);

    void generatePalette(QColor mainColor, int n, Palettes palette=Palettes::Main);

private:
    QColor backgroundColor;
    QColor xAxisBasePenColor;
    QColor yAxisBasePenColor;
    QColor xAxis2BasePenColor;
    QColor yAxis2BasePenColor;
    QColor xAxisTickPenColor;
    QColor yAxisTickPenColor;
    QColor xAxisSubTickPenColor;
    QColor yAxisSubTickPenColor;

    QColor mainColorStyle, secondaryColorStyle;

    QMap<Palettes, Palette> palettes;
};

#endif // MYQCUSTOMPLOT_H
