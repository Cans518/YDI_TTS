#include "app/serialsender.h"
#include "ui/uitheme.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Yidian"));
    QCoreApplication::setApplicationName(QStringLiteral("YDI_TTS"));
    QCoreApplication::setApplicationVersion(QStringLiteral(YDI_TTS_VERSION));
    UiTheme::install(app);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({ QStringLiteral("smoke-test"), QStringLiteral("启动界面后自动退出，用于安装验证。") });
    parser.process(app);

    SerialSender window;
    window.show();
    if (parser.isSet(QStringLiteral("smoke-test")))
        QTimer::singleShot(350, &app, &QCoreApplication::quit);
    return app.exec();
}
