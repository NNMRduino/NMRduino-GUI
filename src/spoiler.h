#ifndef SPOILER_H
#define SPOILER_H

#include <QParallelAnimationGroup>
#include <QScrollArea>
#include <QLabel>
#include <QWidget>
#include <QPointer>

class QToolButton;

class Spoiler : public QWidget {
    Q_OBJECT
private:
    QParallelAnimationGroup toggleAnimation;
    //QScrollArea contentArea;
    QWidget contentArea;
    QWidget toggleArea;
    int animationDuration{300};
    QPointer<QToolButton> toggleButton;
public:
    explicit Spoiler(const QString & title = "", const int animationDuration = 300, QWidget *parent = 0);
    virtual ~Spoiler() {}
    void addWidget(QWidget* widget);

private:
    void updateAnimation();

private slots:
    void runAnimation(bool forward);
    void animationFinished();
};
#endif // SPOILER_H
