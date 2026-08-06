#pragma once

#include "TvDevice.h"

#include <QObject>
#include <QHash>
#include <QUdpSocket>
#include <QNetworkAccessManager>

class SsdpDiscovery final : public QObject {
    Q_OBJECT
public:
    explicit SsdpDiscovery(QObject *parent = nullptr);
    void start(int durationMs = 3500);

signals:
    void deviceFound(const TvDevice &device);
    void finished();
    void statusChanged(const QString &status);

private slots:
    void readPendingDatagrams();

private:
    void inspectLocation(const QUrl &location, const QString &fallbackIp);
    void emitFallback(const QString &ip);

    QUdpSocket socket_;
    QNetworkAccessManager network_;
    QHash<QString, bool> seenIps_;
};
