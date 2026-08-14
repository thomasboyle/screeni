#pragma once

#include <QColor>
#include <QString>

namespace Theme {
inline constexpr QColor PageBg{0xE8, 0xDF, 0xD0};
inline constexpr QColor SidebarBg{0xE0, 0xD8, 0xC8};
inline constexpr QColor Surface{0xF7, 0xF0, 0xE2};
inline constexpr QColor SurfaceHover{0xFF, 0xFB, 0xF3};
inline constexpr QColor SurfacePressed{0xE8, 0xDF, 0xD0};
inline constexpr QColor Frame{0xA7, 0xA2, 0x8B};
inline constexpr QColor Ink{0x2A, 0x32, 0x20};
inline constexpr QColor InkMuted{0x5A, 0x56, 0x4C};
inline constexpr QColor BorderSoft{0xA7, 0xA2, 0x8B};
inline constexpr QColor BorderStrong{0x6B, 0x74, 0x4F};
inline constexpr QColor Accent{0x8F, 0x9A, 0x6E};
inline constexpr QColor AccentDeep{0x6A, 0x74, 0x4C};
inline constexpr QColor AccentTint{0xE8, 0xEC, 0xD9};
inline constexpr QColor AccentFill{0xA8, 0xAA, 0x8C};
inline constexpr QColor AccentFillHover{0xC2, 0xC3, 0xA2};
inline constexpr QColor PlantLight{0xC2, 0xC3, 0xA2};
inline constexpr QColor PlantMid{0x8F, 0x9A, 0x6E};
inline constexpr QColor PlantDark{0x5F, 0x6B, 0x45};

inline constexpr int WindowWidth = 1024;
inline constexpr int WindowHeight = 764;
inline constexpr int SidebarWidth = 272;

inline QString globalStyleSheet()
{
    return QStringLiteral(
        "QWidget { background-color: #E8DFD0; color: #2A3220; font-family: 'Monocraft', 'Consolas', monospace; font-size: 14px; }"
        "QMainWindow, QDialog { background-color: #E8DFD0; }"
        "QLabel { background: transparent; color: #2A3220; }"
        "QLabel#muted { color: #5A564C; }"
        "QLabel#title { font-size: 26px; }"
        "QLabel#section { font-size: 16px; }"
        "QLabel#display { font-size: 36px; }"
        "QPushButton {"
        "  background-color: #F7F0E2; color: #2A3220; border: 1px solid #A7A28B; border-bottom: 2px solid #A7A28B;"
        "  border-radius: 4px; padding: 6px 12px; min-height: 28px;"
        "}"
        "QPushButton:hover { background-color: #FFFBF3; border-color: #6B744F; }"
        "QPushButton:pressed { background-color: #E8DFD0; border: 2px solid #6B744F; }"
        "QPushButton#nav { background: transparent; border: none; border-radius: 4px; text-align: left; padding: 0 14px; min-height: 44px; }"
        "QPushButton#nav:hover { background-color: #C2C3A2; }"
        "QPushButton#navSelected { background-color: #A8AA8C; border: none; border-radius: 4px; text-align: left; padding: 0 14px; min-height: 44px; }"
        "QPushButton#toggle { min-width: 72px; min-height: 40px; }"
        "QPushButton#toggleChecked { background-color: #A8AA8C; border: 1px solid #2A3220; min-width: 72px; min-height: 40px; }"
        "QFrame#card { background-color: #F7F0E2; border: 1px solid #A7A28B; border-radius: 10px; }"
        "QFrame#sidebar { background-color: #E0D8C8; border-right: 1px solid #A7A28B; }"
        "QProgressBar { background: transparent; border: none; border-radius: 4px; max-height: 8px; min-height: 8px; }"
        "QProgressBar::chunk { background-color: #8F9A6E; border-radius: 4px; }"
        "QCheckBox { spacing: 10px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #A7A28B; border-radius: 4px; background: #F7F0E2; }"
        "QCheckBox::indicator:checked { background: #8F9A6E; border-color: #2A3220; }"
        "QSpinBox { background: #F7F0E2; border: 1px solid #A7A28B; border-radius: 4px; padding: 4px; min-height: 28px; }"
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 8px; }"
        "QScrollBar::handle:vertical { background: #A7A28B; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QListWidget { background: transparent; border: none; outline: none; }"
        "QListWidget::item { background: transparent; border-top: 1px solid #A7A28B; min-height: 64px; }"
        "QListWidget::item:selected { background: transparent; }"
    );
}
}  // namespace Theme
