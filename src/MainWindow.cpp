#include "MainWindow.h"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int kRoundButtonSize = 58;
constexpr int kSmallButtonSize = 50;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    buildUi();

    connect(&discovery_, &SsdpDiscovery::deviceFound, this, &MainWindow::addDevice);
    connect(&discovery_, &SsdpDiscovery::statusChanged, statusLabel_, &QLabel::setText);
    connect(&discovery_, &SsdpDiscovery::finished, this, [this] {
        discoverButton_->setEnabled(true);
    });

    connect(&remote_, &SamsungRemote::statusChanged, statusLabel_, &QLabel::setText);
    connect(&remote_, &SamsungRemote::connected, this, [this] {
        const bool wasAutoConnecting = autoConnecting_;
        autoConnecting_ = false;
        updateConnectionState(true);
        saveSelectedTv();
        if (wasAutoConnecting) {
            // Startup auto-connect, or a self-healed reconnect after the
            // remembered TV was found at a new IP — either way this wasn't
            // the user opening the panel on purpose, so tuck it away again.
            showConnectionPanel(false);
        }
    });
    connect(&remote_, &SamsungRemote::disconnected, this, [this] {
        updateConnectionState(false);
    });
    connect(&remote_, &SamsungRemote::pairingRequired, this, [this] {
        QMessageBox::information(
            this,
            QStringLiteral("Approve on TV"),
            QStringLiteral("Use the physical Samsung remote to approve this application on the TV screen."));
    });
    connect(&remote_, &SamsungRemote::tokenReceived, this, [this](const QString &token) {
        const int index = tvCombo_->currentIndex();
        if (index >= 0 && index < devices_.size()) {
            storeToken(devices_[index], token);
        }
    });
    connect(&remote_, &SamsungRemote::errorOccurred, this, [this](const QString &error) {
        if (autoConnecting_) {
            // The remembered TV couldn't be reached (off, moved, etc.) — fall
            // back to letting the user find/pick one instead of popping an
            // error dialog on every ordinary "TV is off" launch.
            autoConnecting_ = false;
            showConnectionPanel(true);
            startDiscovery();
            return;
        }
        QMessageBox::warning(this, QStringLiteral("Samsung TV Remote"), error);
    });

    loadSettings();

    if (!devices_.isEmpty()) {
        showConnectionPanel(false);
        autoConnecting_ = true;
        connectSelectedTv();
    } else {
        showConnectionPanel(true);
        startDiscovery();
    }
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("Samsung TV Remote"));

    QSettings settings;
    const bool compactLayout = settings.value(QStringLiteral("ui/compactLayout"), false).toBool();

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *titleBlock = new QVBoxLayout;
    titleBlock->setSpacing(1);

    auto *title = new QLabel(QStringLiteral("Samsung TV Remote"), central);
    title->setObjectName(QStringLiteral("titleLabel"));
    tvNameLabel_ = new QLabel(QStringLiteral("No TV selected"), central);
    tvNameLabel_->setObjectName(QStringLiteral("tvNameLabel"));
    connectionLabel_ = new QLabel(QStringLiteral("● Disconnected"), central);
    connectionLabel_->setObjectName(QStringLiteral("disconnectedLabel"));

    titleBlock->addWidget(title);
    titleBlock->addWidget(tvNameLabel_);
    titleBlock->addWidget(connectionLabel_);
    header->addLayout(titleBlock, 1);

    setupButton_ = new QPushButton(QStringLiteral("TV setup"), central);
    setupButton_->setObjectName(QStringLiteral("setupButton"));
    header->addWidget(setupButton_, 0, Qt::AlignTop);
    root->addLayout(header);

    connectionPanel_ = buildConnectionPanel();
    layoutCombo_->setCurrentIndex(compactLayout ? 1 : 0);
    root->addWidget(connectionPanel_);

    auto *line = new QFrame(central);
    line->setFrameShape(QFrame::HLine);
    line->setObjectName(QStringLiteral("divider"));
    root->addWidget(line);

    // The remote panel's button grid (D-pad, rockers, etc.) needs more
    // vertical space than fits on most screens at once. Scrolling it keeps
    // every button at full size without ever forcing rows to compress into
    // each other (which is what caused them to overlap).
    remoteScrollArea_ = new QScrollArea(central);
    remoteScrollArea_->setObjectName(QStringLiteral("remoteScrollArea"));
    remoteScrollArea_->setWidgetResizable(true);
    remoteScrollArea_->setFrameShape(QFrame::NoFrame);
    remoteScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    remoteScrollArea_->setAlignment(Qt::AlignHCenter);
    remoteScrollArea_->setWidget(compactLayout ? buildCompactRemotePanel() : buildFullRemotePanel());
    root->addWidget(remoteScrollArea_, 1);

    statusLabel_ = new QLabel(QStringLiteral("Ready."), central);
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    setCentralWidget(central);
    setRemoteEnabled(false);

    resize(470, 900);
    setMinimumSize(410, 600);

    connect(setupButton_, &QPushButton::clicked, this, &MainWindow::toggleConnectionPanel);
    connect(discoverButton_, &QPushButton::clicked, this, &MainWindow::startDiscovery);
    // Connected after the initial setCurrentIndex() above so restoring the
    // saved preference doesn't immediately rebuild the panel it just built.
    connect(layoutCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        setRemoteLayout(index == 1);
    });
    connect(connectButton_, &QPushButton::clicked, this, [this] {
        if (remote_.isConnected()) {
            disconnectTv();
        } else {
            connectSelectedTv();
        }
    });

    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget {
            background: #17191c;
            color: #f2f2f2;
            font-family: "Arial", "Helvetica Neue", sans-serif;
            font-size: 14px;
        }
        QLabel#titleLabel {
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#tvNameLabel {
            color: #c7c9cc;
            font-size: 15px;
        }
        QLabel#connectedLabel { color: #43d868; font-weight: 600; }
        QLabel#disconnectedLabel { color: #9b9ea3; font-weight: 600; }
        QLabel#statusLabel {
            background: #202328;
            border: 1px solid #30343a;
            border-radius: 8px;
            color: #bfc3c8;
            padding: 8px 10px;
        }
        QWidget#connectionPanel {
            background: #202328;
            border: 1px solid #34383f;
            border-radius: 10px;
        }
        QComboBox, QLineEdit {
            background: #292d32;
            border: 1px solid #41464e;
            border-radius: 6px;
            min-height: 30px;
            padding: 2px 8px;
        }
        QPushButton#setupButton, QPushButton#actionButton {
            background: #30343a;
            border: 1px solid #464b53;
            border-radius: 7px;
            padding: 7px 13px;
        }
        QPushButton#setupButton:hover, QPushButton#actionButton:hover {
            background: #3a3f46;
        }
        QPushButton#remoteButton, QPushButton#powerButton,
        QPushButton#appButton, QPushButton#dpadButton, QPushButton#okButton,
        QPushButton#rockerButton {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #34373b, stop:1 #202225);
            border: 2px solid #08090a;
            border-radius: 27px;
            color: #f4f4f4;
            font-size: 17px;
            font-weight: 600;
        }
        QPushButton#remoteButton:hover, QPushButton#powerButton:hover,
        QPushButton#appButton:hover, QPushButton#dpadButton:hover,
        QPushButton#okButton:hover, QPushButton#rockerButton:hover {
            background: #41454a;
        }
        QPushButton#remoteButton:pressed, QPushButton#powerButton:pressed,
        QPushButton#appButton:pressed, QPushButton#dpadButton:pressed,
        QPushButton#okButton:pressed, QPushButton#rockerButton:pressed {
            background: #111315;
            padding-top: 2px;
        }
        QPushButton:disabled {
            color: #65686c;
            background: #242629;
            border-color: #181a1c;
        }
        QPushButton#powerButton { color: #ff4149; font-size: 25px; }
        QPushButton#okButton { font-size: 18px; }
        QPushButton#dpadButton { font-size: 24px; }
        QPushButton#rockerButton { border-radius: 20px; font-size: 15px; }
        QPushButton#appButton { font-size: 12px; }
        QFrame#divider { color: #32363b; }
    )"));
}

QWidget *MainWindow::buildConnectionPanel() {
    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("connectionPanel"));
    auto *form = new QFormLayout(panel);
    form->setContentsMargins(12, 12, 12, 12);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);

    tvCombo_ = new QComboBox(panel);
    tvCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    tvCombo_->setMinimumContentsLength(18);
    discoverButton_ = new QPushButton(QStringLiteral("Discover"), panel);
    discoverButton_->setObjectName(QStringLiteral("actionButton"));
    auto *tvRow = new QHBoxLayout;
    tvRow->addWidget(tvCombo_, 1);
    tvRow->addWidget(discoverButton_);
    form->addRow(QStringLiteral("TV:"), tvRow);

    manualIp_ = new QLineEdit(panel);
    manualIp_->setPlaceholderText(QStringLiteral("Optional IP address, e.g. 192.168.1.50"));
    form->addRow(QStringLiteral("Manual IP:"), manualIp_);

    connectButton_ = new QPushButton(QStringLiteral("Connect and remember"), panel);
    connectButton_->setObjectName(QStringLiteral("actionButton"));
    form->addRow(QString(), connectButton_);

    layoutCombo_ = new QComboBox(panel);
    layoutCombo_->setObjectName(QStringLiteral("layoutCombo"));
    layoutCombo_->addItem(QStringLiteral("Full remote"));
    layoutCombo_->addItem(QStringLiteral("Compact remote"));
    form->addRow(QStringLiteral("Layout:"), layoutCombo_);

    return panel;
}

QWidget *MainWindow::buildFullRemotePanel() {
    auto *remoteBody = new QWidget(this);
    remoteBody->setMaximumWidth(390);
    auto *root = new QVBoxLayout(remoteBody);
    root->setContentsMargins(15, 2, 15, 2);
    root->setSpacing(10);

    // Power — centred, alone at the top
    {
        auto *row = new QHBoxLayout;
        row->addStretch();
        row->addWidget(makeRemoteButton(QStringLiteral("⏻"), QStringLiteral("KEY_POWER"),
                                        QStringLiteral("Power off. Power-on availability varies by model."),
                                        QStringLiteral("powerButton")));
        row->addStretch();
        root->addLayout(row);
    }

    // Source | Menu | Info
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(34);
        row->addStretch();
        row->addWidget(makeRemoteButton(QStringLiteral("↪"), QStringLiteral("KEY_SOURCE"),
                                        QStringLiteral("Select input source.")));
        row->addWidget(makeRemoteButton(QStringLiteral("☰"), QStringLiteral("KEY_MENU"),
                                        QStringLiteral("Open the TV menu.")));
        row->addWidget(makeRemoteButton(QStringLiteral("ⓘ"), QStringLiteral("KEY_INFO"),
                                        QStringLiteral("Show programme or source information.")));
        row->addStretch();
        root->addLayout(row);
    }

    // D-pad
    root->addWidget(buildDirectionalPad(), 0, Qt::AlignHCenter);

    // Back | Home | Play-Pause
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(34);
        row->addStretch();
        row->addWidget(makeRemoteButton(QStringLiteral("↩"), QStringLiteral("KEY_RETURN"),
                                        QStringLiteral("Back/Return.")));
        row->addWidget(makeRemoteButton(QStringLiteral("⌂"), QStringLiteral("KEY_HOME"),
                                        QStringLiteral("Open Samsung Smart Hub/Home.")));
        row->addWidget(makeRemoteButton(QStringLiteral("▶Ⅱ"), QStringLiteral("KEY_PLAY_PAUSE"),
                                        QStringLiteral("Play or pause media.")));
        row->addStretch();
        root->addLayout(row);
    }

    // Rewind | Stop | FF
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(12);
        row->addStretch();
        for (const auto &item : {
                 qMakePair(QStringLiteral("⏪"), QStringLiteral("KEY_REWIND")),
                 qMakePair(QStringLiteral("■"),  QStringLiteral("KEY_STOP")),
                 qMakePair(QStringLiteral("⏩"), QStringLiteral("KEY_FF"))}) {
            auto *btn = makeRemoteButton(item.first, item.second, {});
            btn->setFixedSize(kSmallButtonSize, kSmallButtonSize);
            row->addWidget(btn);
        }
        row->addStretch();
        root->addLayout(row);
    }

    // Vol rocker | CC/AD | CH rocker
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(22);
        row->addStretch();
        row->addWidget(buildRocker(QStringLiteral("+"), QStringLiteral("VOL"), QStringLiteral("−"),
                                   QStringLiteral("KEY_VOLUP"), QStringLiteral("KEY_MUTE"),
                                   QStringLiteral("KEY_VOLDOWN")));
        auto *cc = makeRemoteButton(QStringLiteral("CC/AD"), QStringLiteral("KEY_AD"),
                                    QStringLiteral("Accessibility/Audio Description menu."));
        cc->setFixedSize(68, 58);
        row->addWidget(cc, 0, Qt::AlignCenter);
        row->addWidget(buildRocker(QStringLiteral("⌃"), QStringLiteral("CH"), QStringLiteral("⌄"),
                                   QStringLiteral("KEY_CHUP"), QStringLiteral("KEY_PRECH"),
                                   QStringLiteral("KEY_CHDOWN")));
        row->addStretch();
        root->addLayout(row);
    }

    // 123 | Guide
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(34);
        row->addStretch();
        row->addWidget(makeRemoteButton(QStringLiteral("123"), QStringLiteral("KEY_123"),
                                        QStringLiteral("Open the on-screen number/colour-key panel.")));
        row->addWidget(makeRemoteButton(QStringLiteral("Guide"), QStringLiteral("KEY_GUIDE"),
                                        QStringLiteral("Open the programme guide.")));
        row->addStretch();
        root->addLayout(row);
    }

    // Colour keys
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(7);
        row->addStretch();
        for (const auto &item : {
                 qMakePair(QStringLiteral("R"), QStringLiteral("KEY_RED")),
                 qMakePair(QStringLiteral("G"), QStringLiteral("KEY_GREEN")),
                 qMakePair(QStringLiteral("Y"), QStringLiteral("KEY_YELLOW")),
                 qMakePair(QStringLiteral("B"), QStringLiteral("KEY_BLUE"))}) {
            auto *btn = makeRemoteButton(item.first, item.second,
                                         QStringLiteral("Colour function key."));
            btn->setFixedSize(42, 42);
            row->addWidget(btn);
        }
        row->addStretch();
        root->addLayout(row);
    }

    // App shortcuts
    {
        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(10);
        const struct { const char *label; const char *key; const char *tip; int r; int c; } apps[] = {
            {"NETFLIX",         "KEY_NETFLIX",      "Launch Netflix where supported.",         0, 0},
            {"prime video",     "KEY_AMAZON",       "Launch Prime Video where supported.",     0, 1},
            {"Disney+",         "KEY_DISNEYPLUS",   "Launch Disney+ where supported.",         1, 0},
            {"Samsung\nTV Plus","KEY_SAMSUNGTVPLUS","Launch Samsung TV Plus where supported.", 1, 1},
        };
        for (const auto &a : apps) {
            auto *btn = makeRemoteButton(QString::fromUtf8(a.label), QString::fromUtf8(a.key),
                                         QString::fromUtf8(a.tip), QStringLiteral("appButton"));
            btn->setFixedSize(92, 54);
            grid->addWidget(btn, a.r, a.c);
        }
        auto *wrapper = new QHBoxLayout;
        wrapper->addStretch();
        wrapper->addLayout(grid);
        wrapper->addStretch();
        root->addLayout(wrapper);
    }

    return remoteBody;
}

QWidget *MainWindow::buildCompactRemotePanel() {
    // A trimmed-down "One Remote"-style layout: power, three top keys, D-pad,
    // back/home/play, two-way volume/channel rockers, and a few app keys —
    // no transport-control row, no CC/AD, no colour keys.
    auto *remoteBody = new QWidget(this);
    remoteBody->setMaximumWidth(390);
    auto *root = new QVBoxLayout(remoteBody);
    root->setContentsMargins(15, 2, 15, 2);
    root->setSpacing(16);

    // Power — centred, alone at the top
    {
        auto *row = new QHBoxLayout;
        row->addStretch();
        row->addWidget(makeRemoteButton(QStringLiteral("⏻"), QStringLiteral("KEY_POWER"),
                                        QStringLiteral("Power off. Power-on availability varies by model."),
                                        QStringLiteral("powerButton")));
        row->addStretch();
        root->addLayout(row);
    }

    // 123 | Menu | Guide
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(34);
        row->addStretch();
        row->addWidget(makeRemoteButton(QStringLiteral("123"), QStringLiteral("KEY_123"),
                                        QStringLiteral("Open the on-screen number/colour-key panel.")));
        row->addWidget(makeRemoteButton(QStringLiteral("☰"), QStringLiteral("KEY_MENU"),
                                        QStringLiteral("Open the TV menu.")));
        row->addWidget(makeRemoteButton(QStringLiteral("▤"), QStringLiteral("KEY_GUIDE"),
                                        QStringLiteral("Open the programme guide.")));
        row->addStretch();
        root->addLayout(row);
    }

    // D-pad
    root->addWidget(buildDirectionalPad(), 0, Qt::AlignHCenter);

    // Back | Home | Play-Pause
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(34);
        row->addStretch();
        row->addWidget(makeRemoteButton(QStringLiteral("↩"), QStringLiteral("KEY_RETURN"),
                                        QStringLiteral("Back/Return.")));
        row->addWidget(makeRemoteButton(QStringLiteral("⌂"), QStringLiteral("KEY_HOME"),
                                        QStringLiteral("Open Samsung Smart Hub/Home.")));
        row->addWidget(makeRemoteButton(QStringLiteral("▶Ⅱ"), QStringLiteral("KEY_PLAY_PAUSE"),
                                        QStringLiteral("Play or pause media.")));
        row->addStretch();
        root->addLayout(row);
    }

    // Volume | Channel — same 3-segment rockers as the full remote, so the
    // middle button mutes / jumps to the previous channel like the real one.
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(28);
        row->addStretch();
        row->addWidget(buildRocker(QStringLiteral("+"), QStringLiteral("VOL"), QStringLiteral("−"),
                                   QStringLiteral("KEY_VOLUP"), QStringLiteral("KEY_MUTE"),
                                   QStringLiteral("KEY_VOLDOWN")));
        row->addWidget(buildRocker(QStringLiteral("⌃"), QStringLiteral("CH"), QStringLiteral("⌄"),
                                   QStringLiteral("KEY_CHUP"), QStringLiteral("KEY_PRECH"),
                                   QStringLiteral("KEY_CHDOWN")));
        row->addStretch();
        root->addLayout(row);
    }

    // App shortcuts
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(14);
        row->addStretch();
        const struct { const char *label; const char *key; const char *tip; } apps[] = {
            {"NETFLIX",          "KEY_NETFLIX",       "Launch Netflix where supported."},
            {"prime video",      "KEY_AMAZON",        "Launch Prime Video where supported."},
            {"Samsung\nTV Plus", "KEY_SAMSUNGTVPLUS", "Launch Samsung TV Plus where supported."},
        };
        for (const auto &a : apps) {
            auto *btn = makeRemoteButton(QString::fromUtf8(a.label), QString::fromUtf8(a.key),
                                         QString::fromUtf8(a.tip), QStringLiteral("appButton"));
            btn->setFixedSize(92, 54);
            row->addWidget(btn);
        }
        row->addStretch();
        root->addLayout(row);
    }

    return remoteBody;
}

void MainWindow::setRemoteLayout(bool compact) {
    remoteButtons_.clear();
    if (QWidget *old = remoteScrollArea_->takeWidget()) {
        old->deleteLater();
    }
    remoteScrollArea_->setWidget(compact ? buildCompactRemotePanel() : buildFullRemotePanel());
    setRemoteEnabled(remote_.isConnected());

    QSettings settings;
    settings.setValue(QStringLiteral("ui/compactLayout"), compact);
}

QWidget *MainWindow::buildDirectionalPad() {
    // Use absolute positioning so the oversized OK button never pushes arrow
    // buttons out of their cells (a grid layout would cause overlap here).
    constexpr int kPadSize  = 240;
    constexpr int kOkSize   = 88;
    constexpr int kArrSize  = 62;
    constexpr int kEdge     = 9;
    constexpr int kCenter   = kPadSize / 2;  // 120

    auto *pad = new QWidget(this);
    pad->setObjectName(QStringLiteral("dPad"));
    pad->setFixedSize(kPadSize, kPadSize);
    // Selector scoped to #dPad so child buttons inherit from the global stylesheet.
    pad->setStyleSheet(QStringLiteral(
        "QWidget#dPad { background: #24272b; border: 2px solid #08090a; border-radius: %1px; }"
    ).arg(kCenter));

    auto *up    = makeRemoteButton(QStringLiteral("⌃"), QStringLiteral("KEY_UP"),
                                   QStringLiteral("Navigate up."),    QStringLiteral("dpadButton"));
    auto *left  = makeRemoteButton(QStringLiteral("‹"), QStringLiteral("KEY_LEFT"),
                                   QStringLiteral("Navigate left."),  QStringLiteral("dpadButton"));
    auto *ok    = makeRemoteButton(QStringLiteral("OK"), QStringLiteral("KEY_ENTER"),
                                   QStringLiteral("Select/OK."),      QStringLiteral("okButton"));
    auto *right = makeRemoteButton(QStringLiteral("›"), QStringLiteral("KEY_RIGHT"),
                                   QStringLiteral("Navigate right."), QStringLiteral("dpadButton"));
    auto *down  = makeRemoteButton(QStringLiteral("⌄"), QStringLiteral("KEY_DOWN"),
                                   QStringLiteral("Navigate down."),  QStringLiteral("dpadButton"));

    for (auto *btn : {up, left, right, down}) {
        btn->setParent(pad);
        btn->setFixedSize(kArrSize, kArrSize);
    }
    ok->setParent(pad);
    ok->setFixedSize(kOkSize, kOkSize);

    ok->move(kCenter - kOkSize / 2,  kCenter - kOkSize / 2);    // centre
    up->move(kCenter - kArrSize / 2, kEdge);                     // top
    down->move(kCenter - kArrSize / 2, kPadSize - kArrSize - kEdge); // bottom
    left->move(kEdge,  kCenter - kArrSize / 2);                  // left
    right->move(kPadSize - kArrSize - kEdge, kCenter - kArrSize / 2); // right

    return pad;
}

QWidget *MainWindow::buildRocker(const QString &topText,
                                 const QString &middleText,
                                 const QString &bottomText,
                                 const QString &topKey,
                                 const QString &middleKey,
                                 const QString &bottomKey) {
    auto *container = new QWidget(this);
    container->setFixedSize(78, 166);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *top = makeRemoteButton(topText, topKey, {}, QStringLiteral("rockerButton"));
    auto *middle = makeRemoteButton(middleText, middleKey, {}, QStringLiteral("rockerButton"));
    auto *bottom = makeRemoteButton(bottomText, bottomKey, {}, QStringLiteral("rockerButton"));
    top->setFixedSize(74, 52);
    middle->setFixedSize(74, 52);
    bottom->setFixedSize(74, 52);
    layout->addWidget(top);
    layout->addWidget(middle);
    layout->addWidget(bottom);
    return container;
}

QPushButton *MainWindow::makeRemoteButton(const QString &text,
                                          const QString &key,
                                          const QString &toolTip,
                                          const QString &objectName) {
    auto *button = new QPushButton(text, this);
    button->setObjectName(objectName);
    button->setFixedSize(kRoundButtonSize, kRoundButtonSize);
    button->setFocusPolicy(Qt::NoFocus);
    if (!toolTip.isEmpty()) {
        button->setToolTip(toolTip);
    }
    if (!key.isEmpty()) {
        connectKeyButton(button, key);
        remoteButtons_.append(button);
    }
    return button;
}

void MainWindow::connectKeyButton(QPushButton *button, const QString &key) {
    connect(button, &QPushButton::clicked, this, [this, key] {
        remote_.sendKey(key);
        statusLabel_->setText(QStringLiteral("Sent %1").arg(key));
    });
}

void MainWindow::setRemoteEnabled(bool enabled) {
    for (QPushButton *button : remoteButtons_) {
        button->setEnabled(enabled);
    }
}

void MainWindow::loadSettings() {
    QSettings settings;
    const QString ip = settings.value(QStringLiteral("selectedTv/ip")).toString();
    const QString name = settings.value(QStringLiteral("selectedTv/name"),
                                        QStringLiteral("Saved Samsung TV")).toString();
    const QString model = settings.value(QStringLiteral("selectedTv/model")).toString();
    const QString id = settings.value(QStringLiteral("selectedTv/id")).toString();
    savedTvId_ = id;
    if (!ip.isEmpty()) {
        addDevice(TvDevice{name, ip, model, id});
        tvCombo_->setCurrentIndex(0);
        tvNameLabel_->setText(QStringLiteral("%1 · %2").arg(name, ip));
    }
}

void MainWindow::saveSelectedTv() {
    const int index = tvCombo_->currentIndex();
    if (index < 0 || index >= devices_.size()) {
        return;
    }
    const TvDevice &tv = devices_[index];
    QSettings settings;
    settings.setValue(QStringLiteral("selectedTv/ip"), tv.ip);
    settings.setValue(QStringLiteral("selectedTv/name"), tv.name);
    settings.setValue(QStringLiteral("selectedTv/model"), tv.model);
    settings.setValue(QStringLiteral("selectedTv/id"), tv.id);
    savedTvId_ = tv.id;
    tvNameLabel_->setText(QStringLiteral("%1 · %2").arg(tv.name, tv.ip));
}

void MainWindow::addDevice(const TvDevice &device) {
    for (int i = 0; i < devices_.size(); ++i) {
        const bool sameId = !device.id.isEmpty() && devices_[i].id == device.id;
        if (sameId || devices_[i].ip == device.ip) {
            const bool relocated = sameId && devices_[i].ip != device.ip && device.id == savedTvId_;
            devices_[i] = device;
            tvCombo_->setItemText(i, device.displayName());
            if (relocated) {
                handleRememberedTvRelocated(device);
            }
            return;
        }
    }
    devices_.append(device);
    tvCombo_->addItem(device.displayName());
}

void MainWindow::handleRememberedTvRelocated(const TvDevice &device) {
    // Samsung's device id is stable across DHCP/IP changes even though the
    // socket address isn't, so a discovery hit matching our remembered id
    // means "same TV, moved" rather than a new device to pick manually.
    QSettings settings;
    settings.setValue(QStringLiteral("selectedTv/ip"), device.ip);
    settings.setValue(QStringLiteral("selectedTv/name"), device.name);
    settings.setValue(QStringLiteral("selectedTv/model"), device.model);
    statusLabel_->setText(QStringLiteral("Found %1 at a new address — reconnecting…").arg(device.name));

    if (!remote_.isConnected()) {
        autoConnecting_ = true;
        remote_.connectToTv(device.ip, tokenFor(device));
    }
}

void MainWindow::connectSelectedTv() {
    const QString manual = manualIp_->text().trimmed();
    if (!manual.isEmpty()) {
        addDevice(TvDevice{QStringLiteral("Manually added Samsung TV"), manual, {}, {}});
        tvCombo_->setCurrentIndex(devices_.size() - 1);
    }

    const int index = tvCombo_->currentIndex();
    if (index < 0 || index >= devices_.size()) {
        QMessageBox::warning(this, QStringLiteral("No TV selected"),
                             QStringLiteral("Discover a TV or enter its IP address first."));
        return;
    }

    const TvDevice &tv = devices_[index];
    tvNameLabel_->setText(QStringLiteral("%1 · %2").arg(tv.name, tv.ip));
    remote_.connectToTv(tv.ip, tokenFor(tv));
}

void MainWindow::disconnectTv() {
    remote_.disconnectFromTv();
}

void MainWindow::toggleConnectionPanel() {
    showConnectionPanel(!connectionPanel_->isVisible());
}

void MainWindow::showConnectionPanel(bool show) {
    connectionPanel_->setVisible(show);
    setupButton_->setText(show ? QStringLiteral("Hide setup") : QStringLiteral("TV setup"));
}

void MainWindow::startDiscovery() {
    discoverButton_->setEnabled(false);
    discovery_.start();
}

QString MainWindow::tokenFor(const TvDevice &tv) const {
    QSettings settings;
    // Tokens are keyed by the TV's stable id where we have one, since that
    // survives IP changes; fall back to the legacy IP-keyed entry both for
    // devices with no id yet (e.g. manually-entered IPs) and to pick up
    // tokens saved before the id was tracked at all.
    if (!tv.id.isEmpty()) {
        const QString byId = settings.value(QStringLiteral("tokens/id/%1").arg(tv.id)).toString();
        if (!byId.isEmpty()) {
            return byId;
        }
    }
    return settings.value(QStringLiteral("tokens/%1").arg(tv.ip)).toString();
}

void MainWindow::storeToken(const TvDevice &tv, const QString &token) {
    QSettings settings;
    const QString key = tv.id.isEmpty() ? QStringLiteral("tokens/%1").arg(tv.ip)
                                        : QStringLiteral("tokens/id/%1").arg(tv.id);
    settings.setValue(key, token);
}

void MainWindow::updateConnectionState(bool connected) {
    setRemoteEnabled(connected);
    connectButton_->setText(connected ? QStringLiteral("Disconnect")
                                      : QStringLiteral("Connect and remember"));
    connectionLabel_->setText(connected ? QStringLiteral("● Connected")
                                        : QStringLiteral("● Disconnected"));
    connectionLabel_->setObjectName(connected ? QStringLiteral("connectedLabel")
                                              : QStringLiteral("disconnectedLabel"));
    connectionLabel_->style()->unpolish(connectionLabel_);
    connectionLabel_->style()->polish(connectionLabel_);
}
