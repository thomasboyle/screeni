#include "SettingsPanel.h"

#include "theme.h"

#include <QCheckBox>
#include <QComboBox>
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
    card_->setStyleSheet(QStringLiteral("QFrame#card { background:%1; border:1px solid %2; border-radius:10px; }")
                             .arg(Theme::palette().surface.name(), Theme::palette().borderStrong.name()));
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

    auto* themeLab = new QLabel(QStringLiteral("Theme"));
    themeLab->setObjectName(QStringLiteral("muted"));
    lay->addWidget(themeLab);

    themeBox_ = new QComboBox;
    themeBox_->addItem(QStringLiteral("Matcha"), static_cast<int>(Theme::Id::Matcha));
    themeBox_->addItem(QStringLiteral("Lilac"), static_cast<int>(Theme::Id::Lilac));
    themeBox_->setCurrentIndex(static_cast<int>(Theme::Id::Matcha));
    applyThemeBoxStyle();
    lay->addWidget(themeBox_);

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
void SettingsPanel::setTheme(Theme::Id id) { themeBox_->setCurrentIndex(static_cast<int>(id)); }
bool SettingsPanel::startWithWindows() const { return startBox_->isChecked(); }
int SettingsPanel::idleThresholdSec() const { return idleSpin_->value(); }
Theme::Id SettingsPanel::theme() const { return static_cast<Theme::Id>(themeBox_->currentIndex()); }

void SettingsPanel::retheme()
{
    card_->setStyleSheet(QStringLiteral("QFrame#card { background:%1; border:1px solid %2; border-radius:10px; }")
                             .arg(Theme::palette().surface.name(), Theme::palette().borderStrong.name()));
    applyThemeBoxStyle();
}

void SettingsPanel::applyThemeBoxStyle()
{
    if (!themeBox_)
        return;
    const auto& P = Theme::palette();
    themeBox_->setStyleSheet(QStringLiteral(
        "QComboBox { background:%1; color:%2; border:1px solid %3; border-radius:4px; padding:4px 8px; min-height:30px; }"
        "QComboBox::drop-down { border:none; width:20px; }"
        "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3; border-radius:4px; padding:4px; outline:none; selection-background-color:%5; selection-color:%2; }"
        "QComboBox QAbstractItemView::item { min-height:30px; padding:6px 8px; background:%1; }"
        "QComboBox QAbstractItemView::item:hover, QComboBox QAbstractItemView::item:selected { background:%5; }")
                                .arg(P.surface.name(), P.ink.name(), P.borderSoft.name(), P.surface.name(), P.accentFill.name()));
}

void SettingsPanel::mousePressEvent(QMouseEvent* event)
{
    if (!card_->geometry().contains(event->pos()))
        emit closeRequested();
    QWidget::mousePressEvent(event);
}
