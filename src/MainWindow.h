#pragma once

#include "SamsungRemote.h"
#include "SsdpDiscovery.h"
#include "TvDevice.h"

#include <QMainWindow>
#include <QVector>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildUi();
    QWidget *buildConnectionPanel();
    QWidget *buildFullRemotePanel();
    QWidget *buildCompactRemotePanel();
    QWidget *buildDirectionalPad();
    QWidget *buildRocker(const QString &topText,
                         const QString &middleText,
                         const QString &bottomText,
                         const QString &topKey,
                         const QString &middleKey,
                         const QString &bottomKey);
    void setRemoteLayout(bool compact);

    QPushButton *makeRemoteButton(const QString &text,
                                  const QString &key = {},
                                  const QString &toolTip = {},
                                  const QString &objectName = QStringLiteral("remoteButton"));
    void connectKeyButton(QPushButton *button, const QString &key);
    void setRemoteEnabled(bool enabled);

    void loadSettings();
    void saveSelectedTv();
    void addDevice(const TvDevice &device);
    void handleRememberedTvRelocated(const TvDevice &device);
    void connectSelectedTv();
    void disconnectTv();
    void toggleConnectionPanel();
    void showConnectionPanel(bool show);
    void startDiscovery();
    QString tokenFor(const TvDevice &tv) const;
    void storeToken(const TvDevice &tv, const QString &token);
    void updateConnectionState(bool connected);

    SsdpDiscovery discovery_;
    SamsungRemote remote_;
    QVector<TvDevice> devices_;
    QVector<QPushButton *> remoteButtons_;
    bool autoConnecting_ = false;
    QString savedTvId_;

    QWidget *connectionPanel_ = nullptr;
    QPushButton *setupButton_ = nullptr;
    QComboBox *tvCombo_ = nullptr;
    QLineEdit *manualIp_ = nullptr;
    QPushButton *discoverButton_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QComboBox *layoutCombo_ = nullptr;
    QScrollArea *remoteScrollArea_ = nullptr;
    QLabel *tvNameLabel_ = nullptr;
    QLabel *connectionLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
};
