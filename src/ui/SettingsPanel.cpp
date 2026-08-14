#include "SettingsPanel.h"

#include "theme.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsPanel::SettingsPanel(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("background-color: transparent;"));

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 34, 35, 0);
    outer->addStretch();

    card_ = new QFrame;
    card_->setObjectName(QStringLiteral("card"));
    card_->setFixedWidth(340);
    card_->setStyleSheet(QStringLiteral(
        "QFrame#card { background:#F7F0E2; border:1px solid #6B744F; border-radius:10px; }"));
    auto* lay = new QVBoxLayout(card_);
    lay->setContentsMargins(20, 18, 20, 18);
    lay->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("Settings"));
    title->setObjectName(QStringLiteral("section"));
    lay->addWidget(title);

    startBox_ = new QCheckBox(QStringLiteral("Start with Windows"));
    lay->addWidget(startBox_);

    auto* idleLab = new QLabel(QStringLiteral("Idle pause (seconds)"));
    idleLab->setObjectName(QStringLiteral("muted"));
    lay->addWidget(idleLab);

    idleSpin_ = new QSpinBox;
    idleSpin_->setRange(15, 3600);
    idleSpin_->setValue(60);
    lay->addWidget(idleSpin_);

    auto* apply = new QPushButton(QStringLiteral("Apply"));
    auto* clear = new QPushButton(QStringLiteral("Clear usage data"));
    auto* close = new QPushButton(QStringLiteral("Close"));
    connect(apply, &QPushButton::clicked, this, &SettingsPanel::applyRequested);
    connect(clear, &QPushButton::clicked, this, &SettingsPanel::clearDataRequested);
    connect(close, &QPushButton::clicked, this, &SettingsPanel::closeRequested);
    lay->addWidget(apply);
    lay->addWidget(clear);
    lay->addWidget(close);

    outer->addWidget(card_, 0, Qt::AlignTop);
}

void SettingsPanel::setStartWithWindows(bool on) { startBox_->setChecked(on); }
void SettingsPanel::setIdleThresholdSec(int sec) { idleSpin_->setValue(sec); }
bool SettingsPanel::startWithWindows() const { return startBox_->isChecked(); }
int SettingsPanel::idleThresholdSec() const { return idleSpin_->value(); }

void SettingsPanel::mousePressEvent(QMouseEvent* event)
{
    if (!card_->geometry().contains(event->pos()))
        emit closeRequested();
    QWidget::mousePressEvent(event);
}
