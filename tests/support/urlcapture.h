#pragma once

#include <QDesktopServices>
#include <QList>
#include <QObject>
#include <QUrl>

// Capture real desktop-service requests without launching a browser during tests.
class UrlCapture : public QObject
{
    Q_OBJECT
public:
    QList<QUrl> opened;
    UrlCapture()
    {
        QDesktopServices::setUrlHandler(QStringLiteral("https"), this, "record");
        QDesktopServices::setUrlHandler(QStringLiteral("http"), this, "record");
    }
    ~UrlCapture() override
    {
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
        QDesktopServices::unsetUrlHandler(QStringLiteral("http"));
    }
public slots:
    void record(const QUrl &url) { opened.append(url); }
};
