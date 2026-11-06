#ifndef UITHEME_H
#define UITHEME_H

#include <QColor>
#include <QIcon>

class QApplication;
namespace UiTheme {
// Bright paper surfaces and pastel accents with matching dark text.
namespace Colors {
    inline const QColor Surface { "#fffefa" };
    inline const QColor SurfaceShade { "#f6faf5" };
    inline const QColor Border { "#d4dfd1" };
    inline const QColor Text { "#304638" };
    inline const QColor Muted { "#5c705e" };
    inline const QColor Accent { "#52785f" };
    inline const QColor Focus { "#88a68e" };
    inline const QColor Soft { "#f0f5ed" };
    inline const QColor SoftHover { "#e3ecdf" };
    inline const QColor DisabledSurface { "#f4f6f1" };
    inline const QColor DisabledText { "#8b9888" };
    inline const QColor Blue { "#c9dfeb" };
    inline const QColor BlueText { "#304e63" };
    inline const QColor Green { "#d7e8d3" };
    inline const QColor GreenText { "#39573f" };
    inline const QColor Amber { "#f1e1c6" };
    inline const QColor AmberText { "#694d2b" };
    inline const QColor Red { "#f1d6cd" };
    inline const QColor RedText { "#71433c" };
    inline const QColor Purple { "#e3d7ee" };
    inline const QColor PurpleText { "#58446d" };
    inline const QColor Teal { "#d0e7e2" };
    inline const QColor TealText { "#2e5752" };
}
void install(QApplication &app);
bool reducedMotion();
void setReducedMotion(bool reduced);
QIcon refreshIcon();
}

#endif
