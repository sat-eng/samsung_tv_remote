#include "SamsungRemote.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSslConfiguration>
#include <QUrlQuery>

SamsungRemote::SamsungRemote(QObject *parent) : QObject(parent) {
    connect(&socket_, &QWebSocket::connected, this, [this] {
        emit statusChanged(QStringLiteral("Connected to TV."));
        emit connected();
    });
    connect(&socket_, &QWebSocket::disconnected, this, &SamsungRemote::disconnected);
    connect(&socket_, &QWebSocket::textMessageReceived, this, &SamsungRemote::processMessage);
    connect(&socket_, &QWebSocket::sslErrors, this, [this](const QList<QSslError> &) {
        // Samsung TVs normally use a self-signed certificate on port 8002.
        socket_.ignoreSslErrors();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(&socket_, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
#else
    connect(&socket_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, [this](QAbstractSocket::SocketError) {
#endif
        if (!triedInsecure_) {
            triedInsecure_ = true;
            emit statusChanged(QStringLiteral("Secure connection failed; trying port 8001…"));
            open(false);
            return;
        }
        emit errorOccurred(socket_.errorString());
        emit statusChanged(QStringLiteral("Connection failed: %1").arg(socket_.errorString()));
    });
}

void SamsungRemote::connectToTv(const QString &ip, const QString &savedToken) {
    ip_ = ip.trimmed();
    token_ = savedToken;
    triedInsecure_ = false;
    socket_.abort();
    open(true);
}

void SamsungRemote::open(bool secure) {
    const QByteArray appName = QByteArray("CLion Samsung TV Remote").toBase64();
    QUrl url;
    url.setScheme(secure ? QStringLiteral("wss") : QStringLiteral("ws"));
    url.setHost(ip_);
    url.setPort(secure ? 8002 : 8001);
    url.setPath(QStringLiteral("/api/v2/channels/samsung.remote.control"));

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("name"), QString::fromLatin1(appName));
    if (!token_.isEmpty()) query.addQueryItem(QStringLiteral("token"), token_);
    url.setQuery(query);

    emit statusChanged(QStringLiteral("Connecting to %1…").arg(ip_));
    socket_.open(url);
}

void SamsungRemote::disconnectFromTv() {
    socket_.close();
}

void SamsungRemote::sendKey(const QString &key) {
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("The TV is not connected."));
        return;
    }

    QJsonObject params{
        {QStringLiteral("Cmd"), QStringLiteral("Click")},
        {QStringLiteral("DataOfCmd"), key},
        {QStringLiteral("Option"), QStringLiteral("false")},
        {QStringLiteral("TypeOfRemote"), QStringLiteral("SendRemoteKey")}
    };
    QJsonObject command{
        {QStringLiteral("method"), QStringLiteral("ms.remote.control")},
        {QStringLiteral("params"), params}
    };
    socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(command).toJson(QJsonDocument::Compact)));
}

void SamsungRemote::processMessage(const QString &message) {
    const QJsonObject root = QJsonDocument::fromJson(message.toUtf8()).object();
    const QString event = root.value(QStringLiteral("event")).toString();

    if (event == QStringLiteral("ms.channel.connect")) {
        const QJsonObject data = root.value(QStringLiteral("data")).toObject();
        const QString receivedToken = data.value(QStringLiteral("token")).toString();
        if (!receivedToken.isEmpty() && receivedToken != token_) {
            token_ = receivedToken;
            emit tokenReceived(token_);
        }
    } else if (event == QStringLiteral("ms.channel.unauthorized")) {
        emit pairingRequired();
        emit statusChanged(QStringLiteral("Approve the connection request shown on the TV."));
    }
}
