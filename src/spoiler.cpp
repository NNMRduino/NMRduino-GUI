/* adapted from https://stackoverflow.com/a/37119983 */

#include "spoiler.h"

#include <QPropertyAnimation>
#include <QDebug>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

Spoiler::Spoiler(const QString &, const int animationDuration, QWidget *parent) :
    QWidget(parent), animationDuration(animationDuration), toggleButton(new QToolButton)
{
    //auto toggleButton = new QToolButton();
    toggleButton->setStyleSheet("QToolButton { border: none; }");
    toggleButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toggleButton->setArrowType(Qt::ArrowType::DownArrow);
    toggleButton->setCheckable(true);
    toggleButton->setChecked(false);

    auto headerLine = new QFrame();
    headerLine->setFrameShape(QFrame::VLine);
    headerLine->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding);

    auto toggleAreaLayout = new QVBoxLayout();
    toggleAreaLayout->setContentsMargins(0, 0, 0, 0);
    toggleAreaLayout->setAlignment(Qt::AlignHCenter);
    toggleAreaLayout->addWidget(toggleButton, 0, Qt::AlignHCenter);
    toggleAreaLayout->addWidget(headerLine, 0, Qt::AlignHCenter);

    toggleArea.setLayout(toggleAreaLayout);
    toggleArea.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto contentLayout = new QGridLayout();
    contentLayout->setSpacing(0);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    contentArea.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    // start out collapsed
    contentArea.setMaximumWidth(0);
    contentArea.setMinimumWidth(0);
    contentArea.setLayout(contentLayout);

    // let the entire widget grow and shrink with its content
    toggleAnimation.addAnimation(new QPropertyAnimation(this, "minimumWidth"));
    toggleAnimation.addAnimation(new QPropertyAnimation(this, "maximumWidth"));
    toggleAnimation.addAnimation(new QPropertyAnimation(&contentArea, "maximumWidth"));

    // don't waste space
    auto mainLayout = new QHBoxLayout();
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(&toggleArea);
    mainLayout->addWidget(&contentArea);

    setLayout(mainLayout);

    connect(toggleButton.data(), &QToolButton::clicked, this, &Spoiler::runAnimation);
    connect(&toggleAnimation, &QParallelAnimationGroup::finished, this, &Spoiler::animationFinished);

    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
}

void Spoiler::addWidget(QWidget * widget) {

    //delete contentArea.layout();
    //contentArea.setLayout(contentLayout);
    contentArea.layout()->addWidget(widget);
}

void Spoiler::updateAnimation()
{
    const auto collapsedWidth = toggleArea.sizeHint().width();
    //const auto contentWidth = contentArea.layout()->sizeHint().width();
    const auto contentWidth = contentArea.sizeHint().width();

    for (int i = 0; i < toggleAnimation.animationCount() - 1; ++i) {
        QPropertyAnimation * spoilerAnimation = static_cast<QPropertyAnimation *>(toggleAnimation.animationAt(i));
        spoilerAnimation->setDuration(animationDuration);
        spoilerAnimation->setStartValue(collapsedWidth);
        spoilerAnimation->setEndValue(collapsedWidth + contentWidth);
    }

    QPropertyAnimation * contentAnimation = static_cast<QPropertyAnimation *>(toggleAnimation.animationAt(toggleAnimation.animationCount() - 1));
    contentAnimation->setDuration(animationDuration);
    contentAnimation->setStartValue(0);
    contentAnimation->setEndValue(contentWidth);
}

void Spoiler::runAnimation(bool forward)
{
    updateAnimation();
    if (forward){
        toggleButton->setArrowType(Qt::ArrowType::RightArrow);
        toggleAnimation.setDirection(QAbstractAnimation::Forward);
        toggleAnimation.start();
    } else {
        toggleButton->setArrowType(Qt::ArrowType::DownArrow);
        toggleAnimation.setDirection(QAbstractAnimation::Backward);
        toggleAnimation.start();
    }
}

void Spoiler::animationFinished()
{
    if (toggleButton->isChecked()){
        contentArea.setMaximumWidth(QWIDGETSIZE_MAX);
        this->setMaximumWidth(QWIDGETSIZE_MAX);
        this->setMinimumWidth(0);
    }
}
