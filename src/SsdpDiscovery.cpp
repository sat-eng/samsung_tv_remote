#include "SsdpDiscovery.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

SsdpDiscovery::SsdpDiscovery(QObject *parent) : QObject(parent) {
    connect(&socket_, &QUdpSocket::readyRead, this, &SsdpDiscovery::readPendingDatagrams);
}

void SsdpDiscovery::start(int durationMs) {
    seenIps_.clear();
    socket_.close();
    if (!socket_.bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit statusChanged(QStringLiteral("Could not open the UDP discovery socket: %1").arg(socket_.errorString()));
        emit finished();
        return;
    }

    static const QByteArray request =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: ssdp:all\r\n\r\n";

    socket_.writeDatagram(request, QHostAddress(QStringLiteral("239.255.255.250")), 1900);
    emit statusChanged(QStringLiteral("Searching the local network for Samsung TVs…"));

    QTimer::singleShot(durationMs, this, [this] {
        socket_.close();
        emit statusChanged(seenIps_.isEmpty()
            ? QStringLiteral("No TVs found. Ensure the TV is on and on the same network, or add its IP address manually.")
            : QStringLiteral("Discovery complete."));
        emit finished();
    });
}

void SsdpDiscovery::readPendingDatagrams() {
    while (socket_.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = socket_.receiveDatagram();
        const QByteArray response = datagram.data();
        const QString text = QString::fromUtf8(response);
        const QString lower = text.toLower();

        // Samsung TVs commonly advertise "samsung", MediaRenderer, or MainTVAgent services.
        if (!lower.contains(QStringLiteral("samsung")) &&
            !lower.contains(QStringLiteral("mediarenderer")) &&
            !lower.contains(QStringLiteral("maintvagent"))) {
            continue;
        }

        const QString ip = datagram.senderAddress().toString();
        if (seenIps_.contains(ip)) continue;
        seenIps_.insert(ip, true);

        QString location;
        const auto lines = text.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            if (line.startsWith(QStringLiteral("LOCATION:"), Qt::CaseInsensitive)) {
                location = line.mid(line.indexOf(':') + 1).trimmed();
                break;
            }
        }

        if (!location.isEmpty()) inspectLocation(QUrl(location), ip);
        else emitFallback(ip);
    }
}

void SsdpDiscovery::inspectLocation(const QUrl &, const QString &fallbackIp) {
    // Samsung Tizen TVs expose useful identity data at /api/v2/.
    QNetworkRequest request(QUrl(QStringLiteral("http://%1:8001/api/v2/").arg(fallbackIp)));
    request.setTransferTimeout(1500);
    QNetworkReply *reply = network_.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, fallbackIp] {
        TvDevice device;
        device.ip = fallbackIp;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            const QJsonObject root = document.object();
            const QJsonObject dev = root.value(QStringLiteral("device")).toObject();
            device.name = dev.value(QStringLiteral("name")).toString();
            device.model = dev.value(QStringLiteral("modelName")).toString();
            device.id = dev.value(QStringLiteral("id")).toString();
        }
        if (device.name.isEmpty()) device.name = QStringLiteral("Samsung TV");
        emit deviceFound(device);
        reply->deleteLater();
    });
}

void SsdpDiscovery::emitFallback(const QString &ip) {
    TvDevice device;
    device.name = QStringLiteral("Samsung TV");
    device.ip = ip;
    emit deviceFound(device);
}
