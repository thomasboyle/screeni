#pragma once

#include "theme.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QSpinBox;

class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget* parent = nullptr);
    void setStartWithWindows(bool on);
    void setIdleThresholdSec(int sec);
    void setTheme(Theme::Id id);
    bool startWithWindows() const;
    int idleThresholdSec() const;
    Theme::Id theme() const;
    void retheme();
    void applyThemeBoxStyle();
signals:
    void applyRequested();
    void clearDataRequested();
    void closeRequested();
protected:
    void mousePressEvent(QMouseEvent* event) override;
private:
    QCheckBox* startBox_ = nullptr;
    QSpinBox* idleSpin_ = nullptr;
    QComboBox* themeBox_ = nullptr;
    QWidget* card_ = nullptr;
};