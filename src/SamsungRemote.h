#pragma once

#include <QObject>
#include <QWebSocket>

class SamsungRemote final : public QObject {
    Q_OBJECT
public:
    explicit SamsungRemote(QObject *parent = nullptr);

    void connectToTv(const QString &ip, const QString &savedToken = {});
    void disconnectFromTv();
    void sendKey(const QString &key);
    [[nodiscard]] QString token() const { return token_; }
    [[nodiscard]] bool isConnected() const { return socket_.state() == QAbstractSocket::ConnectedState; }

signals:
    void connected();
    void disconnected();
    void pairingRequired();
    void tokenReceived(const QString &token);
    void statusChanged(const QString &status);
    void errorOccurred(const QString &message);

private:
    void open(bool secure);
    void processMessage(const QString &message);

    QWebSocket socket_;
    QString ip_;
    QString token_;
    bool triedInsecure_ = false;
};
