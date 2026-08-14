#pragma once

#include <QColor>
#include <QString>

namespace Theme {

enum class Id { Matcha, Lilac };

struct Palette {
    QColor pageBg;
    QColor sidebarBg;
    QColor surface;
    QColor surfaceHover;
    QColor surfacePressed;
    QColor frame;
    QColor ink;
    QColor inkMuted;
    QColor borderSoft;
    QColor borderStrong;
    QColor accent;
    QColor accentDeep;
    QColor accentTint;
    QColor accentFill;
    QColor accentFillHover;
    QColor plantLight;
    QColor plantMid;
    QColor plantDark;
    QColor sidebarInk;
    QColor sidebarInkMuted;
};

inline const Palette& matchaPalette()
{
    static const Palette p{
        {0xE8, 0xDF, 0xD0},  // pageBg
        {0x43, 0x4B, 0x2D},  // sidebarBg  (dark green nav bar)
        {0xF7, 0xF0, 0xE2},  // surface
        {0xFF, 0xFB, 0xF3},  // surfaceHover
        {0xE8, 0xDF, 0xD0},  // surfacePressed
        {0xA7, 0xA2, 0x8B},  // frame
        {0x2A, 0x32, 0x20},  // ink
        {0x5A, 0x56, 0x4C},  // inkMuted
        {0xA7, 0xA2, 0x8B},  // borderSoft
        {0x6B, 0x74, 0x4F},  // borderStrong
        {0x8F, 0x9A, 0x6E},  // accent
        {0x6A, 0x74, 0x4C},  // accentDeep
        {0xE8, 0xEC, 0xD9},  // accentTint
        {0xA8, 0xAA, 0x8C},  // accentFill
        {0xC2, 0xC3, 0xA2},  // accentFillHover
        {0xC2, 0xC3, 0xA2},  // plantLight
        {0x8F, 0x9A, 0x6E},  // plantMid
        {0x5F, 0x6B, 0x45},  // plantDark
        {0xF7, 0xF0, 0xE2},  // sidebarInk  (light text on dark nav bar)
        {0xC2, 0xC3, 0xA2},  // sidebarInkMuted
    };
    return p;
}

inline const Palette& lilacPalette()
{
    static const Palette p{
        {0xEC, 0xE6, 0xF7},  // pageBg  pastel lilac
        {0xE2, 0xDB, 0xF0},  // sidebarBg
        {0xF7, 0xF3, 0xFD},  // surface
        {0xFC, 0xFA, 0xFF},  // surfaceHover
        {0xE9, 0xE1, 0xF5},  // surfacePressed
        {0xB9, 0xA8, 0xD8},  // frame
        {0x2D, 0x24, 0x40},  // ink
        {0x5B, 0x52, 0x70},  // inkMuted
        {0xB9, 0xA8, 0xD8},  // borderSoft
        {0x7E, 0x6A, 0xA6},  // borderStrong
        {0x9D, 0x8C, 0xC4},  // accent
        {0x7E, 0x6A, 0xA6},  // accentDeep
        {0xEC, 0xE5, 0xF7},  // accentTint
        {0xC2, 0xB5, 0xDE},  // accentFill
        {0xD5, 0xCB, 0xEA},  // accentFillHover
        {0xC2, 0xB5, 0xDE},  // plantLight
        {0x9D, 0x8C, 0xC4},  // plantMid
        {0x6E, 0x5C, 0x96},  // plantDark
        {0x2D, 0x24, 0x40},  // sidebarInk
        {0x5B, 0x52, 0x70},  // sidebarInkMuted
    };
    return p;
}

inline Id& currentId()
{
    static Id id = Id::Matcha;
    return id;
}

inline const Palette& palette()
{
    return currentId() == Id::Lilac ? lilacPalette() : matchaPalette();
}

inline void setTheme(Id id) { currentId() = id; }

inline QString themeName(Id id)
{
    return id == Id::Lilac ? QStringLiteral("Lilac") : QStringLiteral("Matcha");
}

inline constexpr int WindowWidth = 1024;
inline constexpr int WindowHeight = 764;
inline constexpr int SidebarWidth = 216;

inline QString globalStyleSheet()
{
    const auto& P = palette();
    QString s = QStringLiteral(
        "QWidget { background-color: %1; color: %2; font-family: 'Monocraft', 'Consolas', monospace; font-size: 14px; }"
        "QMainWindow, QDialog { background-color: %3; }"
        "QLabel { background: transparent; color: %4; }"
        "QLabel#muted { color: %5; }"
        "QLabel#title { font-size: 26px; }"
        "QLabel#section { font-size: 16px; }"
        "QLabel#display { font-size: 36px; }"
        "QPushButton {"
        "  background-color: %6; color: %7; border: 1px solid %8; border-bottom: 2px solid %9;"
        "  border-radius: 4px; padding: 6px 12px; min-height: 28px;"
        "}"
        "QPushButton:hover { background-color: %10; border-color: %11; }"
        "QPushButton:pressed { background-color: %12; border: 2px solid %13; }"
        "QFrame#sidebar QPushButton#nav { color: %36; background: transparent; border: none; border-radius: 4px; text-align: center; padding: 0 14px; min-height: 44px; }"
        "QFrame#sidebar QPushButton#nav:hover { background-color: %38; color: %40; }"
        "QFrame#sidebar QPushButton#nav[navSelected=\"true\"] { background-color: %39; color: %40; border: none; border-left: 4px solid %41; border-right: 4px solid %41; border-radius: 4px; text-align: center; padding: 0 10px; min-height: 44px; }"
        "QPushButton#toggle { min-width: 72px; min-height: 40px; }"
        "QPushButton#toggleChecked { background-color: %17; border: 1px solid %18; min-width: 72px; min-height: 40px; }"
        "QFrame#card { background-color: %19; border: 1px solid %20; border-radius: 10px; }"
        "QFrame#sidebar { background-color: %21; border-right: 1px solid %22; }"
        "QFrame#sidebar QLabel { color: %36; }"
        "QFrame#sidebar QLabel#muted { color: %37; }"
        "QFrame#sidebar QFrame#updateBubble QLabel { color: %2; }"
        "QFrame#sidebar QFrame#updateBubble QLabel#muted { color: %5; }"
        "QProgressBar { background: transparent; border: none; border-radius: 4px; max-height: 8px; min-height: 8px; }"
        "QProgressBar::chunk { background-color: %23; border-radius: 4px; }"
        "QCheckBox { spacing: 10px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid %24; border-radius: 4px; background: %25; }"
        "QCheckBox::indicator:checked { background: %26; border-color: %27; }"
        "QSpinBox { background: %28; border: 1px solid %29; border-radius: 4px; padding: 4px; min-height: 28px; }"
        "QComboBox { background: %32; color: %33; border: 1px solid %34; border-radius: 4px; padding: 4px 8px; min-height: 30px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: %32; color: %33; border: 1px solid %34; border-radius: 4px; padding: 4px; outline: none; selection-background-color: %35; selection-color: %33; }"
        "QComboBox QAbstractItemView::item { min-height: 30px; padding: 6px 8px; background: transparent; }"
        "QComboBox QAbstractItemView::item:hover { background: %35; }"
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 8px; }"
        "QScrollBar::handle:vertical { background: %30; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QListWidget { background: transparent; border: none; outline: none; }"
        "QListWidget::item { background: transparent; border-top: 1px solid %31; min-height: 64px; }"
        "QListWidget::item:selected { background: transparent; }"
    );
    return s.arg(P.pageBg.name())                 // %1
        .arg(P.ink.name())                        // %2
        .arg(P.pageBg.name())                     // %3
        .arg(P.ink.name())                        // %4
        .arg(P.inkMuted.name())                   // %5
        .arg(P.surface.name())                    // %6
        .arg(P.ink.name())                        // %7
        .arg(P.borderSoft.name())                 // %8
        .arg(P.borderSoft.name())                 // %9
        .arg(P.surfaceHover.name())               // %10
        .arg(P.borderStrong.name())               // %11
        .arg(P.surfacePressed.name())             // %12
        .arg(P.borderStrong.name())               // %13
        .arg(P.accentFill.name())                 // %17
        .arg(P.ink.name())                        // %18
        .arg(P.surface.name())                    // %19
        .arg(P.borderSoft.name())                 // %20
        .arg(P.sidebarBg.name())                  // %21
        .arg(P.borderSoft.name())                 // %22
        .arg(P.accent.name())                     // %23
        .arg(P.borderSoft.name())                 // %24
        .arg(P.surface.name())                    // %25
        .arg(P.accent.name())                     // %26
        .arg(P.ink.name())                        // %27
        .arg(P.surface.name())                    // %28
        .arg(P.borderSoft.name())                 // %29
        .arg(P.borderSoft.name())                 // %30
        .arg(P.borderSoft.name())                 // %31
        .arg(P.surface.name())                    // %32
        .arg(P.ink.name())                        // %33
        .arg(P.borderSoft.name())                 // %34
        .arg(P.accentFill.name())                 // %35
        .arg(P.sidebarInk.name())                 // %36
        .arg(P.sidebarInkMuted.name())            // %37
        .arg(P.plantLight.name())                 // %38
        .arg(P.accentFill.name())                 // %39
        .arg(P.ink.name())                        // %40
        .arg(P.plantDark.name());                 // %41
}
}  // namespace Theme