#include "ttstests.h"
#include "ui/uitheme.h"
#include <QFontInfo>
#include <QRawFont>
#include <QSettings>

void TtsTests::initTestCase()
{
    QVERIFY(settings.isValid());
    QCoreApplication::setOrganizationName(QStringLiteral("YDI-Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("TTS-Tests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    QSettings().setValue(QStringLiteral("appearance/reducedMotion"), true);
    UiTheme::install(*qApp);
    QVERIFY(!UiTheme::reducedMotion()); // A retired setting must not turn animations off.
    const QRawFont suppliedFont(QStringLiteral(":/fonts/LXGWWenKaiMonoGBScreen.ttf"), 13);
    QVERIFY(suppliedFont.isValid());
    QCOMPARE(QFontInfo(qApp->font()).family(), suppliedFont.familyName());
    qInfo().noquote() << "UI font:" << QFontInfo(qApp->font()).family()
                      << "requested:" << qApp->font().families().join(", ");
}

void TtsTests::cleanup()
{
    UiTheme::setReducedMotion(false);
    QSettings().clear();
}

QTEST_MAIN(TtsTests)
