#include "myqcustomplot.h"

using namespace Plot;

MyQCustomPlot::MyQCustomPlot(QWidget *parent):
    QCustomPlot(parent)
{
    show(); // Ugly hack to load stylesheet at time of object creation
#ifdef QCUSTOMPLOT_USE_OPENGL
    setOpenGl(true);
    if (not QCustomPlot::openGl()){
        qCritical() << "Couldn't load OpenGL";
    }
#endif
}

MyQCustomPlot::~MyQCustomPlot()
{

}

QColor MyQCustomPlot::getColorFromPalette(int i, int n, Palettes palette)
{
    switch(palette){
    case Palettes::Main:
        return getColorFromPalette(i, n, mainColorStyle, palette);
    case Palettes::Secondary:
        return getColorFromPalette(i, n, secondaryColorStyle, palette);
    case Palettes::None:
        return getColorFromPalette(i, n, QColor(), palette);
    }
    return getColorFromPalette(i, n, QColor(), palette);
}

QColor MyQCustomPlot::getColorFromPalette(int i, int n, QColor mainColor, Palettes palette)
{
    generatePalette(mainColor, n, palette);
    return palettes.value(palette).colors.value(i);
}

void MyQCustomPlot::setBackgroundColor(QColor color)
{
    backgroundColor = color;
    setBackground(color);
}

void MyQCustomPlot::setXAxisBasePenColor(QColor color)
{
    xAxisBasePenColor = color;

    auto pen = xAxis->property("basePen").value<QPen>();
    pen.setColor(color);
    xAxis->setBasePen(pen);
}

void MyQCustomPlot::setYAxisBasePenColor(QColor color)
{
    yAxisBasePenColor = color;

    auto pen = yAxis->property("basePen").value<QPen>();
    pen.setColor(color);
    yAxis->setBasePen(pen);
}

void MyQCustomPlot::setXAxis2BasePenColor(QColor color)
{
    xAxis2BasePenColor = color;

    auto pen = xAxis2->property("basePen").value<QPen>();
    pen.setColor(color);
    xAxis2->setBasePen(pen);
}

void MyQCustomPlot::setYAxis2BasePenColor(QColor color)
{
    yAxis2BasePenColor = color;

    auto pen = yAxis2->property("basePen").value<QPen>();
    pen.setColor(color);
    yAxis2->setBasePen(pen);
}

void MyQCustomPlot::setXAxisTickPenColor(QColor color)
{
    xAxisTickPenColor = color;

    auto pen = xAxis->property("tickPen").value<QPen>();
    pen.setColor(color);
    xAxis->setTickPen(pen);
}

void MyQCustomPlot::setYAxisTickPenColor(QColor color)
{
    yAxisTickPenColor = color;

    auto pen = yAxis->property("tickPen").value<QPen>();
    pen.setColor(color);
    yAxis->setTickPen(pen);
}

void MyQCustomPlot::setXAxisSubTickPenColor(QColor color)
{
    xAxisSubTickPenColor = color;

    auto pen = xAxis->property("subTickPen").value<QPen>();
    pen.setColor(color);
    xAxis->setSubTickPen(pen);
}

void MyQCustomPlot::setYAxisSubTickPenColor(QColor color)
{
    yAxisSubTickPenColor = color;

    auto pen = yAxis->property("subTickPen").value<QPen>();
    pen.setColor(color);
    yAxis->setSubTickPen(pen);
}

void MyQCustomPlot::setGraph0PenColor(QColor color)
{
    mainColorStyle = color;
}

void MyQCustomPlot::setGraph1PenColor(QColor color)
{
    secondaryColorStyle = color;
}

void MyQCustomPlot::generatePalette(QColor mainColor, int n, Palettes palette)
{
    Palette newPalette = palettes.value(palette, Palette());
    if ((newPalette.mainColor == mainColor) and (newPalette.colors.length() == n)){
        return;
    }

    newPalette.mainColor = mainColor;
    newPalette.colors.clear();

    if (n < 1){
        return;
    } else if (n == 1){
        newPalette.colors.append(mainColor);
        palettes.insert(palette, newPalette);
        return;
    }

    float weightLast, weightMain;
    int r, g, b;

    QColor lastColor(192, 192, 192, 255);
    for (int i=0; i<n; i++){
        weightLast = float(i) / (n-1);
        weightMain = 1 - weightLast;

        r = round(weightMain*mainColor.red()   + weightLast*lastColor.red());
        g = round(weightMain*mainColor.green() + weightLast*lastColor.green());
        b = round(weightMain*mainColor.blue()  + weightLast*lastColor.blue());

        newPalette.colors.append(QColor(r, g, b, 255));
    }
    palettes.insert(palette, newPalette);
}
