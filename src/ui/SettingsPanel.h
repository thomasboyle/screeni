#pragma once

#include <QWidget>

class QCheckBox;
class QSpinBox;

class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget* parent = nullptr);
    void setStartWithWindows(bool on);
    void setIdleThresholdSec(int sec);
    bool startWithWindows() const;
    int idleThresholdSec() const;
signals:
    void applyRequested();
    void clearDataRequested();
    void closeRequested();
protected:
    void mousePressEvent(QMouseEvent* event) override;
private:
    QCheckBox* startBox_ = nullptr;
    QSpinBox* idleSpin_ = nullptr;
    QWidget* card_ = nullptr;
};
