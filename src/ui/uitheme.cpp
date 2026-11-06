#include "ui/uitheme.h"
#include "ui/hoverhint.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QPainter>
#include <QPalette>

void UiTheme::install(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/LXGWWenKaiMonoGBScreen.ttf"));
    const auto families = QFontDatabase::applicationFontFamilies(fontId);
    if (!families.isEmpty())
        font.setFamily(families.first());
    font.setPixelSize(13);
    font.setWeight(QFont::Normal);
    // Grayscale antialiasing avoids colored subpixel fringes on the translucent window.
    font.setStyleStrategy(
        QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality | QFont::NoSubpixelAntialias));
    font.setHintingPreference(QFont::PreferVerticalHinting);
    app.setFont(font);

    QPalette palette;
    palette.setColor(QPalette::Window, Colors::SurfaceShade);
    palette.setColor(QPalette::WindowText, Colors::Text);
    palette.setColor(QPalette::Base, Colors::Surface);
    palette.setColor(QPalette::AlternateBase, Colors::Soft);
    palette.setColor(QPalette::Text, Colors::Text);
    palette.setColor(QPalette::Button, Colors::Surface);
    palette.setColor(QPalette::ButtonText, Colors::Text);
    palette.setColor(QPalette::Mid, Colors::Border);
    palette.setColor(QPalette::Dark, Colors::Muted);
    palette.setColor(QPalette::Highlight, Colors::Accent);
    palette.setColor(QPalette::HighlightedText, Colors::Surface);
    palette.setColor(QPalette::PlaceholderText, Colors::Muted);
    palette.setColor(QPalette::ToolTipBase, Colors::Surface);
    palette.setColor(QPalette::ToolTipText, Colors::Text);
    palette.setColor(QPalette::Disabled, QPalette::Text, Colors::DisabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, Colors::DisabledText);
    app.setPalette(palette);

    QFile stylesheet(QStringLiteral(":/theme.qss"));
    if (stylesheet.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    // The retired UI preference must not silently disable animations on startup.
    setReducedMotion(false);
    installHoverHints(app);
}

bool UiTheme::reducedMotion()
{
    return qApp && qApp->property("reducedMotion").toBool();
}

void UiTheme::setReducedMotion(bool reduced)
{
    if (qApp)
        qApp->setProperty("reducedMotion", reduced);
}

QIcon UiTheme::refreshIcon()
{
    QPixmap pixmap(48, 48);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Colors::Muted, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawArc(QRectF(5, 5, 14, 14), 35 * 16, 285 * 16);
    p.drawPolyline(QPolygonF { QPointF(15, 4), QPointF(19, 6), QPointF(17, 10) });
    return QIcon(pixmap);
}
