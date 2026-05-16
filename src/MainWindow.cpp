#include "MainWindow.hpp"

#include "ChatProtocol.hpp"

#include <QCheckBox>
#include <QCompleter>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace
{
QString bandwidthLabel(int bandwidthHz)
{
    return QStringLiteral("%1 Hz").arg(bandwidthHz);
}

QString utcTimeLabel()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

QVariant currentComboData(const QComboBox *combo, const QVariant &fallback = {})
{
    return combo && combo->currentIndex() >= 0 ? combo->currentData() : fallback;
}

QString comboValue(const QComboBox *combo)
{
    if (!combo)
        return {};

    const int index = combo->currentIndex();
    if (index >= 0 && combo->currentText() == combo->itemText(index))
        return combo->itemData(index).toString().trimmed();

    return combo->currentText().trimmed();
}

void addAudioDeviceItems(QComboBox *combo, const QList<QAudioDevice> &devices)
{
    combo->addItem(QStringLiteral("Default"), QString());

    QStringList seen;
    for (const QAudioDevice &device : devices)
    {
        const QString description = device.description().trimmed();
        if (description.isEmpty() || seen.contains(description, Qt::CaseInsensitive))
            continue;
        seen.append(description);
        combo->addItem(description, description);
    }
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    loadSettings();
    wireSignals();
    setWindowTitle(QStringLiteral("Mercury Chat"));
    QTimer::singleShot(0, this, [this]() {
        if (autoStartModemCheck_->isChecked())
            startModem();
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *outerLayout = new QVBoxLayout(central);
    auto *splitter = new QSplitter(Qt::Horizontal, central);

    auto *leftPanel = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    auto *leftTabs = new QTabWidget(leftPanel);

    auto *modemGroup = new QGroupBox(QStringLiteral("Mercury Modem"), leftPanel);
    auto *modemLayout = new QFormLayout(modemGroup);
    modemPathEdit_ = new QLineEdit(modem_.defaultExecutablePath(), modemGroup);
    modemArgsEdit_ = new QLineEdit(modemGroup);
    modemArgsEdit_->setPlaceholderText(QStringLiteral("Extra Mercury arguments"));
    broadcastPortSpin_ = new QSpinBox(modemGroup);
    broadcastPortSpin_->setRange(1, 65535);
    broadcastPortSpin_->setValue(8100);
    autoStartModemCheck_ = new QCheckBox(QStringLiteral("Start on app launch"), modemGroup);
    autoStartModemCheck_->setChecked(true);
    modemStartButton_ = new QPushButton(QStringLiteral("Start Modem"), modemGroup);
    modemStopButton_ = new QPushButton(QStringLiteral("Stop Modem"), modemGroup);
    modemStopButton_->setEnabled(false);
    auto *modemButtonRow = new QWidget(modemGroup);
    auto *modemButtonLayout = new QHBoxLayout(modemButtonRow);
    modemButtonLayout->setContentsMargins(0, 0, 0, 0);
    modemButtonLayout->addWidget(autoStartModemCheck_);
    modemButtonLayout->addWidget(modemStartButton_);
    modemButtonLayout->addWidget(modemStopButton_);
    modemLayout->addRow(QStringLiteral("Executable"), modemPathEdit_);
    modemLayout->addRow(QStringLiteral("Broadcast port"), broadcastPortSpin_);
    modemLayout->addRow(QStringLiteral("Extra args"), modemArgsEdit_);
    modemLayout->addRow(modemButtonRow);

    auto *audioGroup = new QGroupBox(QStringLiteral("Audio I/O"), leftPanel);
    auto *audioLayout = new QFormLayout(audioGroup);
    soundSystemCombo_ = new QComboBox(audioGroup);
    soundSystemCombo_->addItem(QStringLiteral("Auto"), QStringLiteral("auto"));
    soundSystemCombo_->addItem(QStringLiteral("CoreAudio"), QStringLiteral("coreaudio"));
    soundSystemCombo_->addItem(QStringLiteral("ALSA"), QStringLiteral("alsa"));
    soundSystemCombo_->addItem(QStringLiteral("PulseAudio"), QStringLiteral("pulse"));
    soundSystemCombo_->addItem(QStringLiteral("DirectSound"), QStringLiteral("dsound"));
    soundSystemCombo_->addItem(QStringLiteral("WASAPI"), QStringLiteral("wasapi"));
    soundSystemCombo_->addItem(QStringLiteral("OSS"), QStringLiteral("oss"));
    soundSystemCombo_->addItem(QStringLiteral("AAudio"), QStringLiteral("aaudio"));
    soundSystemCombo_->addItem(QStringLiteral("Shared memory"), QStringLiteral("shm"));
    inputDeviceCombo_ = new QComboBox(audioGroup);
    inputDeviceCombo_->setEditable(true);
    inputDeviceCombo_->setInsertPolicy(QComboBox::NoInsert);
    addAudioDeviceItems(inputDeviceCombo_, QMediaDevices::audioInputs());
    outputDeviceCombo_ = new QComboBox(audioGroup);
    outputDeviceCombo_->setEditable(true);
    outputDeviceCombo_->setInsertPolicy(QComboBox::NoInsert);
    addAudioDeviceItems(outputDeviceCombo_, QMediaDevices::audioOutputs());
    captureChannelCombo_ = new QComboBox(audioGroup);
    captureChannelCombo_->addItem(QStringLiteral("Left"), QStringLiteral("left"));
    captureChannelCombo_->addItem(QStringLiteral("Right"), QStringLiteral("right"));
    captureChannelCombo_->addItem(QStringLiteral("Stereo"), QStringLiteral("stereo"));
    audioLayout->addRow(QStringLiteral("Sound system"), soundSystemCombo_);
    audioLayout->addRow(QStringLiteral("Input device"), inputDeviceCombo_);
    audioLayout->addRow(QStringLiteral("Output device"), outputDeviceCombo_);
    audioLayout->addRow(QStringLiteral("RX channel"), captureChannelCombo_);

    auto *stationGroup = new QGroupBox(QStringLiteral("Station / TNC"), leftPanel);
    auto *stationLayout = new QFormLayout(stationGroup);

    callsignEdit_ = new QLineEdit(stationGroup);
    callsignEdit_->setPlaceholderText(QStringLiteral("N0CALL"));
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"), stationGroup);

    basePortSpin_ = new QSpinBox(stationGroup);
    basePortSpin_->setRange(1, 65534);
    basePortSpin_->setValue(8300);

    bandwidthCombo_ = new QComboBox(stationGroup);
    bandwidthCombo_->addItem(QStringLiteral("BW2300"), 2300);
    bandwidthCombo_->addItem(QStringLiteral("BW500"), 500);
    bandwidthCombo_->addItem(QStringLiteral("BW2750"), 2750);

    tncConnectButton_ = new QPushButton(QStringLiteral("Connect TNC"), stationGroup);
    stationInitButton_ = new QPushButton(QStringLiteral("Initialize"), stationGroup);
    linkDisconnectButton_ = new QPushButton(QStringLiteral("Disconnect Link"), stationGroup);
    stationInitButton_->setEnabled(false);
    linkDisconnectButton_->setEnabled(false);

    auto *stationButtonRow = new QWidget(stationGroup);
    auto *stationButtonLayout = new QHBoxLayout(stationButtonRow);
    stationButtonLayout->setContentsMargins(0, 0, 0, 0);
    stationButtonLayout->addWidget(tncConnectButton_);
    stationButtonLayout->addWidget(stationInitButton_);
    stationButtonLayout->addWidget(linkDisconnectButton_);

    stationLayout->addRow(QStringLiteral("Callsign"), callsignEdit_);
    stationLayout->addRow(QStringLiteral("Mercury host"), hostEdit_);
    stationLayout->addRow(QStringLiteral("ARQ base port"), basePortSpin_);
    stationLayout->addRow(QStringLiteral("Bandwidth"), bandwidthCombo_);
    stationLayout->addRow(stationButtonRow);

    auto *statusGroup = new QGroupBox(QStringLiteral("Status"), leftPanel);
    auto *statusLayout = new QFormLayout(statusGroup);
    tncStatusLabel_ = new QLabel(QStringLiteral("Disconnected"), statusGroup);
    modemStatusLabel_ = new QLabel(QStringLiteral("Stopped"), statusGroup);
    linkStatusLabel_ = new QLabel(QStringLiteral("No ARQ link"), statusGroup);
    pttStatusLabel_ = new QLabel(QStringLiteral("PTT off"), statusGroup);
    bufferStatusLabel_ = new QLabel(QStringLiteral("0 bytes"), statusGroup);
    snrStatusLabel_ = new QLabel(QStringLiteral("-"), statusGroup);
    bitrateStatusLabel_ = new QLabel(QStringLiteral("-"), statusGroup);
    statusLayout->addRow(QStringLiteral("Modem"), modemStatusLabel_);
    statusLayout->addRow(QStringLiteral("TNC"), tncStatusLabel_);
    statusLayout->addRow(QStringLiteral("Link"), linkStatusLabel_);
    statusLayout->addRow(QStringLiteral("PTT"), pttStatusLabel_);
    statusLayout->addRow(QStringLiteral("Buffer"), bufferStatusLabel_);
    statusLayout->addRow(QStringLiteral("SNR"), snrStatusLabel_);
    statusLayout->addRow(QStringLiteral("Bitrate"), bitrateStatusLabel_);

    auto *beaconGroup = new QGroupBox(QStringLiteral("Beacons"), leftPanel);
    auto *beaconLayout = new QVBoxLayout(beaconGroup);
    auto *beaconControlRow = new QWidget(beaconGroup);
    auto *beaconControlLayout = new QHBoxLayout(beaconControlRow);
    beaconControlLayout->setContentsMargins(0, 0, 0, 0);
    beaconSendButton_ = new QPushButton(QStringLiteral("Send Beacon"), beaconGroup);
    autoBeaconCheck_ = new QCheckBox(QStringLiteral("Auto"), beaconGroup);
    beaconSendButton_->setEnabled(false);
    autoBeaconCheck_->setEnabled(false);
    beaconIntervalSpin_ = new QSpinBox(beaconGroup);
    beaconIntervalSpin_->setRange(30, 3600);
    beaconIntervalSpin_->setValue(300);
    beaconIntervalSpin_->setSuffix(QStringLiteral(" s"));
    beaconControlLayout->addWidget(beaconSendButton_);
    beaconControlLayout->addWidget(autoBeaconCheck_);
    beaconControlLayout->addWidget(beaconIntervalSpin_);

    beaconTable_ = new QTableWidget(0, 3, beaconGroup);
    beaconTable_->setHorizontalHeaderLabels({QStringLiteral("Call"), QStringLiteral("BW"), QStringLiteral("Last heard")});
    beaconTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    beaconTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    beaconTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    beaconTable_->horizontalHeader()->setStretchLastSection(true);
    beaconTable_->verticalHeader()->setVisible(false);

    connectBeaconButton_ = new QPushButton(QStringLiteral("Connect Selected"), beaconGroup);
    connectBeaconButton_->setEnabled(false);
    beaconLayout->addWidget(beaconControlRow);
    beaconLayout->addWidget(beaconTable_);
    beaconLayout->addWidget(connectBeaconButton_);

    auto *catGroup = new QGroupBox(QStringLiteral("hamlib CAT"), leftPanel);
    auto *catLayout = new QFormLayout(catGroup);
    catModelCombo_ = new QComboBox(catGroup);
    catModelCombo_->setEditable(true);
    catModelCombo_->setInsertPolicy(QComboBox::NoInsert);
    catModelCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    catModelCombo_->setMinimumContentsLength(24);
    if (catModelCombo_->completer())
        catModelCombo_->completer()->setCompletionMode(QCompleter::PopupCompletion);
    for (const CatRigModel &model : CatController::availableRigModels())
    {
        catModelCombo_->addItem(model.name, model.modelId);
        catModelCombo_->setItemData(catModelCombo_->count() - 1,
                                    QStringLiteral("Hamlib model ID: %1").arg(model.modelId),
                                    Qt::ToolTipRole);
    }
    if (catModelCombo_->count() == 0)
        catModelCombo_->addItem(QStringLiteral("hamlib unavailable"), 0);
    const int dummyIndex = catModelCombo_->findText(QStringLiteral("Hamlib Dummy"), Qt::MatchContains);
    if (dummyIndex >= 0)
        catModelCombo_->setCurrentIndex(dummyIndex);
    catDeviceEdit_ = new QLineEdit(catGroup);
    catDeviceEdit_->setPlaceholderText(QStringLiteral("/dev/ttyUSB0, COM3, or 127.0.0.1:4532"));
    catBaudCombo_ = new QComboBox(catGroup);
    catBaudCombo_->setEditable(true);
    for (const int baud : {0, 4800, 9600, 19200, 38400, 57600, 115200})
        catBaudCombo_->addItem(QString::number(baud), baud);
    catRtsCombo_ = new QComboBox(catGroup);
    catRtsCombo_->addItem(QStringLiteral("Unset"), static_cast<int>(CatSerialLineState::Unset));
    catRtsCombo_->addItem(QStringLiteral("On"), static_cast<int>(CatSerialLineState::On));
    catRtsCombo_->addItem(QStringLiteral("Off"), static_cast<int>(CatSerialLineState::Off));
    catDtrCombo_ = new QComboBox(catGroup);
    catDtrCombo_->addItem(QStringLiteral("Unset"), static_cast<int>(CatSerialLineState::Unset));
    catDtrCombo_->addItem(QStringLiteral("On"), static_cast<int>(CatSerialLineState::On));
    catDtrCombo_->addItem(QStringLiteral("Off"), static_cast<int>(CatSerialLineState::Off));
    catPttMethodCombo_ = new QComboBox(catGroup);
    catPttMethodCombo_->addItem(QStringLiteral("CAT"), static_cast<int>(CatPttMethod::Cat));
    catPttMethodCombo_->addItem(QStringLiteral("RTS"), static_cast<int>(CatPttMethod::SerialRts));
    catPttMethodCombo_->addItem(QStringLiteral("DTR"), static_cast<int>(CatPttMethod::SerialDtr));
    catConnectButton_ = new QPushButton(QStringLiteral("Connect CAT"), catGroup);
    catReadFreqButton_ = new QPushButton(QStringLiteral("Read"), catGroup);
    catFrequencyEdit_ = new QLineEdit(catGroup);
    catFrequencyEdit_->setPlaceholderText(QStringLiteral("Frequency in Hz"));
    catSetFreqButton_ = new QPushButton(QStringLiteral("Set"), catGroup);
    catPttCheck_ = new QCheckBox(QStringLiteral("PTT"), catGroup);
    catFollowPttCheck_ = new QCheckBox(QStringLiteral("Follow modem PTT"), catGroup);
    catFollowPttCheck_->setChecked(true);
    catReadFreqButton_->setEnabled(false);
    catSetFreqButton_->setEnabled(false);
    catPttCheck_->setEnabled(false);
    catFollowPttCheck_->setEnabled(false);

    auto *catConnectRow = new QWidget(catGroup);
    auto *catConnectLayout = new QHBoxLayout(catConnectRow);
    catConnectLayout->setContentsMargins(0, 0, 0, 0);
    catConnectLayout->addWidget(catConnectButton_);
    catConnectLayout->addWidget(catPttCheck_);
    catConnectLayout->addWidget(catFollowPttCheck_);

    auto *catFrequencyRow = new QWidget(catGroup);
    auto *catFrequencyLayout = new QHBoxLayout(catFrequencyRow);
    catFrequencyLayout->setContentsMargins(0, 0, 0, 0);
    catFrequencyLayout->addWidget(catFrequencyEdit_);
    catFrequencyLayout->addWidget(catReadFreqButton_);
    catFrequencyLayout->addWidget(catSetFreqButton_);

    catLayout->addRow(QStringLiteral("Radio"), catModelCombo_);
    catLayout->addRow(QStringLiteral("CAT port"), catDeviceEdit_);
    catLayout->addRow(QStringLiteral("Serial baud"), catBaudCombo_);
    catLayout->addRow(QStringLiteral("RTS"), catRtsCombo_);
    catLayout->addRow(QStringLiteral("DTR"), catDtrCombo_);
    catLayout->addRow(QStringLiteral("PTT method"), catPttMethodCombo_);
    catLayout->addRow(catConnectRow);
    catLayout->addRow(QStringLiteral("Frequency"), catFrequencyRow);

    auto *beaconPage = new QWidget(leftTabs);
    auto *beaconPageLayout = new QVBoxLayout(beaconPage);
    beaconPageLayout->addWidget(beaconGroup);

    auto *modemPage = new QWidget(leftTabs);
    auto *modemPageLayout = new QVBoxLayout(modemPage);
    modemPageLayout->addWidget(modemGroup);
    modemPageLayout->addWidget(audioGroup);
    modemPageLayout->addStretch();

    auto *stationPage = new QWidget(leftTabs);
    auto *stationPageLayout = new QVBoxLayout(stationPage);
    stationPageLayout->addWidget(stationGroup);
    stationPageLayout->addStretch();

    auto *catPage = new QWidget(leftTabs);
    auto *catPageLayout = new QVBoxLayout(catPage);
    catPageLayout->addWidget(catGroup);
    catPageLayout->addStretch();

    auto *statusPage = new QWidget(leftTabs);
    auto *statusPageLayout = new QVBoxLayout(statusPage);
    statusPageLayout->addWidget(statusGroup);
    statusPageLayout->addStretch();

    leftTabs->addTab(beaconPage, QStringLiteral("Beacons"));
    leftTabs->addTab(modemPage, QStringLiteral("Modem"));
    leftTabs->addTab(stationPage, QStringLiteral("Station"));
    leftTabs->addTab(catPage, QStringLiteral("CAT"));
    leftTabs->addTab(statusPage, QStringLiteral("Status"));
    leftLayout->addWidget(leftTabs);

    auto *chatPanel = new QWidget(splitter);
    auto *chatLayout = new QVBoxLayout(chatPanel);
    transcript_ = new QTextEdit(chatPanel);
    transcript_->setReadOnly(true);
    transcript_->setAcceptRichText(false);
    messageEdit_ = new QPlainTextEdit(chatPanel);
    messageEdit_->setPlaceholderText(QStringLiteral("Type UTF-8 text here"));
    messageEdit_->setFixedHeight(96);
    sendButton_ = new QPushButton(QStringLiteral("Send"), chatPanel);
    sendButton_->setEnabled(false);

    chatLayout->addWidget(transcript_, 1);
    chatLayout->addWidget(messageEdit_);
    chatLayout->addWidget(sendButton_);

    splitter->addWidget(leftPanel);
    splitter->addWidget(chatPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({420, 760});

    outerLayout->addWidget(splitter);
    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Mercury will start automatically if the executable is available."));

    beaconTimer_ = new QTimer(this);
    tncRetryTimer_ = new QTimer(this);
    tncRetryTimer_->setInterval(2000);
}

void MainWindow::loadSettings()
{
    QSettings settings;

    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());

    modemPathEdit_->setText(settings.value(QStringLiteral("modem/executable"), modemPathEdit_->text()).toString());
    modemArgsEdit_->setText(settings.value(QStringLiteral("modem/args")).toString());
    broadcastPortSpin_->setValue(settings.value(QStringLiteral("modem/broadcastPort"), broadcastPortSpin_->value()).toInt());
    autoStartModemCheck_->setChecked(settings.value(QStringLiteral("modem/autoStart"), autoStartModemCheck_->isChecked()).toBool());
    setComboCurrentData(soundSystemCombo_, settings.value(QStringLiteral("audio/soundSystem"), currentComboData(soundSystemCombo_, QStringLiteral("auto"))));
    const QString inputDevice = settings.value(QStringLiteral("audio/inputDevice")).toString();
    if (!setComboCurrentData(inputDeviceCombo_, inputDevice))
        inputDeviceCombo_->setCurrentText(inputDevice);
    const QString outputDevice = settings.value(QStringLiteral("audio/outputDevice")).toString();
    if (!setComboCurrentData(outputDeviceCombo_, outputDevice))
        outputDeviceCombo_->setCurrentText(outputDevice);
    setComboCurrentData(captureChannelCombo_, settings.value(QStringLiteral("audio/captureChannel"), currentComboData(captureChannelCombo_, QStringLiteral("left"))));

    callsignEdit_->setText(settings.value(QStringLiteral("station/callsign")).toString());
    hostEdit_->setText(settings.value(QStringLiteral("tnc/host"), hostEdit_->text()).toString());
    basePortSpin_->setValue(settings.value(QStringLiteral("tnc/basePort"), basePortSpin_->value()).toInt());
    setComboCurrentData(bandwidthCombo_, settings.value(QStringLiteral("tnc/bandwidth"), selectedBandwidth()));

    autoBeaconCheck_->setChecked(settings.value(QStringLiteral("beacon/auto"), autoBeaconCheck_->isChecked()).toBool());
    beaconIntervalSpin_->setValue(settings.value(QStringLiteral("beacon/intervalSeconds"), beaconIntervalSpin_->value()).toInt());

    setComboCurrentData(catModelCombo_, settings.value(QStringLiteral("cat/modelId"), currentComboData(catModelCombo_, 0)));
    catDeviceEdit_->setText(settings.value(QStringLiteral("cat/device")).toString());
    const QString savedBaud = settings.value(QStringLiteral("cat/baud"), catBaudCombo_->currentText()).toString();
    if (!setComboCurrentData(catBaudCombo_, savedBaud.toInt()))
        catBaudCombo_->setCurrentText(savedBaud);
    setComboCurrentData(catRtsCombo_, settings.value(QStringLiteral("cat/rts"), currentComboData(catRtsCombo_, 0)));
    setComboCurrentData(catDtrCombo_, settings.value(QStringLiteral("cat/dtr"), currentComboData(catDtrCombo_, 0)));
    setComboCurrentData(catPttMethodCombo_, settings.value(QStringLiteral("cat/pttMethod"), currentComboData(catPttMethodCombo_, 0)));
    catFollowPttCheck_->setChecked(settings.value(QStringLiteral("cat/followModemPtt"), catFollowPttCheck_->isChecked()).toBool());
    catFrequencyEdit_->setText(settings.value(QStringLiteral("cat/frequencyHz"), catFrequencyEdit_->text()).toString());

    if (settings.value(QStringLiteral("window/geometry")).isNull())
        resize(1180, 760);
}

void MainWindow::saveSettings() const
{
    QSettings settings;

    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());

    settings.setValue(QStringLiteral("modem/executable"), modemPathEdit_->text().trimmed());
    settings.setValue(QStringLiteral("modem/args"), modemArgsEdit_->text());
    settings.setValue(QStringLiteral("modem/broadcastPort"), broadcastPortSpin_->value());
    settings.setValue(QStringLiteral("modem/autoStart"), autoStartModemCheck_->isChecked());
    settings.setValue(QStringLiteral("audio/soundSystem"), currentComboData(soundSystemCombo_, QStringLiteral("auto")));
    settings.setValue(QStringLiteral("audio/inputDevice"), comboValue(inputDeviceCombo_));
    settings.setValue(QStringLiteral("audio/outputDevice"), comboValue(outputDeviceCombo_));
    settings.setValue(QStringLiteral("audio/captureChannel"), currentComboData(captureChannelCombo_, QStringLiteral("left")));

    settings.setValue(QStringLiteral("station/callsign"), localCallsign());
    settings.setValue(QStringLiteral("tnc/host"), hostEdit_->text().trimmed());
    settings.setValue(QStringLiteral("tnc/basePort"), basePortSpin_->value());
    settings.setValue(QStringLiteral("tnc/bandwidth"), selectedBandwidth());

    settings.setValue(QStringLiteral("beacon/auto"), autoBeaconCheck_->isChecked());
    settings.setValue(QStringLiteral("beacon/intervalSeconds"), beaconIntervalSpin_->value());

    settings.setValue(QStringLiteral("cat/modelId"), currentComboData(catModelCombo_, 0));
    settings.setValue(QStringLiteral("cat/device"), catDeviceEdit_->text().trimmed());
    settings.setValue(QStringLiteral("cat/baud"), catBaudCombo_->currentText().trimmed());
    settings.setValue(QStringLiteral("cat/rts"), currentComboData(catRtsCombo_, 0));
    settings.setValue(QStringLiteral("cat/dtr"), currentComboData(catDtrCombo_, 0));
    settings.setValue(QStringLiteral("cat/pttMethod"), currentComboData(catPttMethodCombo_, 0));
    settings.setValue(QStringLiteral("cat/followModemPtt"), catFollowPttCheck_->isChecked());
    settings.setValue(QStringLiteral("cat/frequencyHz"), catFrequencyEdit_->text().trimmed());
    settings.sync();
}

void MainWindow::wireSignals()
{
    connect(modemStartButton_, &QPushButton::clicked, this, &MainWindow::startModem);
    connect(modemStopButton_, &QPushButton::clicked, this, &MainWindow::stopModem);
    connect(&modem_, &ModemProcess::runningChanged, this, [this](bool running) {
        modemStatusLabel_->setText(running ? QStringLiteral("Running") : QStringLiteral("Stopped"));
        modemStartButton_->setEnabled(!running);
        modemStopButton_->setEnabled(running);
        modemPathEdit_->setEnabled(!running);
        modemArgsEdit_->setEnabled(!running);
        broadcastPortSpin_->setEnabled(!running);
        soundSystemCombo_->setEnabled(!running);
        inputDeviceCombo_->setEnabled(!running);
        outputDeviceCombo_->setEnabled(!running);
        captureChannelCombo_->setEnabled(!running);
        if (running && hostEdit_->text().trimmed() == QLatin1String("127.0.0.1"))
        {
            tncRetryAttempts_ = 0;
            tncRetryTimer_->start();
        }
        if (!running)
            tncRetryTimer_->stop();
    });
    connect(&modem_, &ModemProcess::statusMessage, this, [this](const QString &message) {
        statusBar()->showMessage(message, 7000);
        appendSystemLine(message);
    });
    connect(&modem_, &ModemProcess::outputLine, this, [this](const QString &line) {
        appendSystemLine(QStringLiteral("modem: %1").arg(line));
    });

    connect(tncConnectButton_, &QPushButton::clicked, this, &MainWindow::connectTnc);
    connect(stationInitButton_, &QPushButton::clicked, this, &MainWindow::initializeStation);
    connect(linkDisconnectButton_, &QPushButton::clicked, &tnc_, &TncClient::disconnectLink);
    connect(beaconSendButton_, &QPushButton::clicked, this, &MainWindow::sendBeacon);
    connect(connectBeaconButton_, &QPushButton::clicked, this, &MainWindow::connectSelectedBeacon);
    connect(beaconTable_, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        connectSelectedBeacon();
    });
    connect(sendButton_, &QPushButton::clicked, this, &MainWindow::sendChatMessage);
    connect(autoBeaconCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (enabled)
        {
            sendBeacon();
            beaconTimer_->start(beaconIntervalSpin_->value() * 1000);
        }
        else
        {
            beaconTimer_->stop();
        }
    });
    connect(beaconIntervalSpin_, &QSpinBox::valueChanged, this, [this](int value) {
        if (autoBeaconCheck_->isChecked())
            beaconTimer_->start(value * 1000);
    });
    connect(beaconTimer_, &QTimer::timeout, this, &MainWindow::sendBeacon);
    connect(tncRetryTimer_, &QTimer::timeout, this, &MainWindow::retryTncConnection);

    connect(&tnc_, &TncClient::connectionStateChanged, this, &MainWindow::updateTncState);
    connect(&tnc_, &TncClient::statusMessage, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
    });
    connect(&tnc_, &TncClient::controlLineReceived, this, [this](const QString &line) {
        appendSystemLine(QStringLiteral("TNC: %1").arg(line));
    });
    connect(&tnc_, &TncClient::cqFrameReceived, this, &MainWindow::onBeaconReceived);
    connect(&tnc_, &TncClient::linkConnected, this, &MainWindow::onLinkConnected);
    connect(&tnc_, &TncClient::linkDisconnected, this, &MainWindow::onLinkDisconnected);
    connect(&tnc_, &TncClient::pendingChanged, this, [this](bool pending) {
        linkStatusLabel_->setText(pending ? QStringLiteral("Pending") : QStringLiteral("Idle"));
    });
    connect(&tnc_, &TncClient::pttChanged, this, [this](bool enabled) {
        pttStatusLabel_->setText(enabled ? QStringLiteral("PTT on") : QStringLiteral("PTT off"));
        if (catFollowPttCheck_->isChecked() && cat_.isConnected())
            cat_.setPtt(enabled);
    });
    connect(&tnc_, &TncClient::bufferUpdated, this, [this](int bytes) {
        bufferStatusLabel_->setText(QStringLiteral("%1 bytes").arg(bytes));
    });
    connect(&tnc_, &TncClient::snrUpdated, this, [this](double snr) {
        snrStatusLabel_->setText(QStringLiteral("%1 dB").arg(snr, 0, 'f', 1));
    });
    connect(&tnc_, &TncClient::bitrateUpdated, this, [this](int level, int bps) {
        bitrateStatusLabel_->setText(QStringLiteral("L%1 %2 bps").arg(level).arg(bps));
    });
    connect(&tnc_, &TncClient::dataReceived, this, &MainWindow::onDataReceived);

    connect(catConnectButton_, &QPushButton::clicked, this, &MainWindow::connectOrDisconnectCat);
    connect(catReadFreqButton_, &QPushButton::clicked, &cat_, &CatController::refreshFrequency);
    connect(catSetFreqButton_, &QPushButton::clicked, this, [this]() {
        cat_.setFrequencyHz(catFrequencyEdit_->text().trimmed().toLongLong());
    });
    connect(catPttCheck_, &QCheckBox::toggled, &cat_, &CatController::setPtt);
    connect(&cat_, &CatController::connectedChanged, this, [this](bool connected) {
        catConnectButton_->setText(connected ? QStringLiteral("Disconnect CAT") : QStringLiteral("Connect CAT"));
        catModelCombo_->setEnabled(!connected);
        catDeviceEdit_->setEnabled(!connected);
        catBaudCombo_->setEnabled(!connected);
        catRtsCombo_->setEnabled(!connected);
        catDtrCombo_->setEnabled(!connected);
        catPttMethodCombo_->setEnabled(!connected);
        catReadFreqButton_->setEnabled(connected);
        catSetFreqButton_->setEnabled(connected);
        catPttCheck_->setEnabled(connected);
        catFollowPttCheck_->setEnabled(connected);
        if (!connected)
        {
            const QSignalBlocker blocker(catPttCheck_);
            catPttCheck_->setChecked(false);
        }
    });
    connect(&cat_, &CatController::statusMessage, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
        appendSystemLine(message);
    });
    connect(&cat_, &CatController::frequencyChanged, this, [this](qint64 frequencyHz) {
        catFrequencyEdit_->setText(QString::number(frequencyHz));
    });
    connect(&cat_, &CatController::pttChanged, this, [this](bool enabled) {
        catPttCheck_->setChecked(enabled);
    });
}

void MainWindow::startModem()
{
    saveSettings();

    QStringList arguments;
    arguments << QStringLiteral("-p") << QString::number(basePortSpin_->value());
    arguments << QStringLiteral("-b") << QString::number(broadcastPortSpin_->value());
    const QString soundSystem = soundSystemCombo_->currentData().toString();
    if (!soundSystem.isEmpty() && soundSystem != QLatin1String("auto"))
        arguments << QStringLiteral("-x") << soundSystem;
    const QString inputDevice = comboValue(inputDeviceCombo_);
    if (!inputDevice.isEmpty())
        arguments << QStringLiteral("-i") << inputDevice;
    const QString outputDevice = comboValue(outputDeviceCombo_);
    if (!outputDevice.isEmpty())
        arguments << QStringLiteral("-o") << outputDevice;
    arguments << QStringLiteral("-k") << captureChannelCombo_->currentData().toString();
    arguments.append(QProcess::splitCommand(modemArgsEdit_->text()));

    modem_.start(modemPathEdit_->text(), arguments);
}

void MainWindow::stopModem()
{
    if (tnc_.isControlConnected() || tnc_.isDataConnected())
        tnc_.disconnectFromModem();
    modem_.stop();
}

void MainWindow::connectTnc()
{
    saveSettings();

    if (tnc_.isControlConnected() || tnc_.isDataConnected())
    {
        tncRetryTimer_->stop();
        tnc_.disconnectFromModem();
        return;
    }

    tncRetryAttempts_ = 0;
    tncRetryTimer_->start();
    tnc_.connectToModem(hostEdit_->text().trimmed(), static_cast<quint16>(basePortSpin_->value()));
}

void MainWindow::retryTncConnection()
{
    if (tnc_.isControlConnected())
    {
        tncRetryTimer_->stop();
        return;
    }

    if (tncRetryAttempts_ >= 20)
    {
        tncRetryTimer_->stop();
        appendSystemLine(QStringLiteral("TNC connection retry stopped; modem control port is still unavailable."));
        return;
    }

    ++tncRetryAttempts_;
    tnc_.connectToModem(hostEdit_->text().trimmed(), static_cast<quint16>(basePortSpin_->value()));
}

bool MainWindow::applyStationSettings(bool warnIfMissing)
{
    const QString callsign = localCallsign();
    if (callsign.isEmpty())
    {
        if (warnIfMissing)
            QMessageBox::warning(this, QStringLiteral("Callsign required"), QStringLiteral("Set a valid local callsign first."));
        return false;
    }

    if (!tnc_.isControlConnected())
    {
        if (warnIfMissing)
            appendSystemLine(QStringLiteral("Station settings not applied: TNC control port is not connected."));
        return false;
    }

    tnc_.initializeStation(callsign, selectedBandwidth());
    saveSettings();
    return true;
}

void MainWindow::initializeStation()
{
    if (applyStationSettings(true))
        appendSystemLine(QStringLiteral("Initialized %1 at %2").arg(localCallsign(), bandwidthLabel(selectedBandwidth())));
}

void MainWindow::sendBeacon()
{
    const QString callsign = localCallsign();
    if (callsign.isEmpty())
    {
        autoBeaconCheck_->setChecked(false);
        QMessageBox::warning(this, QStringLiteral("Callsign required"), QStringLiteral("Set a valid local callsign before sending a beacon."));
        return;
    }

    if (!tnc_.isControlConnected())
    {
        appendSystemLine(QStringLiteral("Beacon not sent: TNC control port is not connected."));
        if (modem_.isRunning())
        {
            tncRetryAttempts_ = 0;
            tncRetryTimer_->start();
        }
        return;
    }

    if (!applyStationSettings(false))
        return;

    tnc_.sendCqFrame(callsign, selectedBandwidth());
    appendSystemLine(QStringLiteral("Beacon sent as %1 %2").arg(callsign, bandwidthLabel(selectedBandwidth())));
}

void MainWindow::connectSelectedBeacon()
{
    const QList<QTableWidgetItem *> selected = beaconTable_->selectedItems();
    if (selected.isEmpty())
        return;

    const int row = selected.first()->row();
    const QString target = beaconTable_->item(row, 0)->text();
    const QString callsign = localCallsign();
    if (callsign.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("Callsign required"), QStringLiteral("Set a valid local callsign before connecting."));
        return;
    }

    if (!tnc_.isControlConnected())
    {
        appendSystemLine(QStringLiteral("Connect not started: TNC control port is not connected."));
        if (modem_.isRunning())
        {
            tncRetryAttempts_ = 0;
            tncRetryTimer_->start();
        }
        return;
    }

    if (!applyStationSettings(false))
        return;

    tnc_.connectPeer(callsign, target);
    appendSystemLine(QStringLiteral("Connecting %1 -> %2").arg(callsign, target));
}

void MainWindow::sendChatMessage()
{
    const QString text = messageEdit_->toPlainText();
    if (text.trimmed().isEmpty())
        return;

    const QString callsign = localCallsign();
    tnc_.sendPayload(ChatProtocol::encodeTextMessage(callsign, text));
    appendTranscript(callsign, text);
    messageEdit_->clear();
}

void MainWindow::onBeaconReceived(const QString &callsign, int bandwidthHz)
{
    if (callsign.isEmpty() || callsign == localCallsign())
        return;

    updateBeaconRow(callsign, bandwidthHz);
    appendSystemLine(QStringLiteral("Beacon heard from %1 %2").arg(callsign, bandwidthLabel(bandwidthHz)));
}

void MainWindow::onLinkConnected(const QString &source, const QString &destination, int bandwidthHz)
{
    arqConnected_ = true;
    const QString local = localCallsign();
    peerCallsign_ = (source == local) ? destination : source;
    linkStatusLabel_->setText(QStringLiteral("Connected to %1 (%2)").arg(peerCallsign_, bandwidthLabel(bandwidthHz)));
    linkDisconnectButton_->setEnabled(true);
    sendButton_->setEnabled(true);
    appendSystemLine(QStringLiteral("ARQ connected: %1 -> %2, %3").arg(source, destination, bandwidthLabel(bandwidthHz)));
}

void MainWindow::onLinkDisconnected()
{
    arqConnected_ = false;
    peerCallsign_.clear();
    linkStatusLabel_->setText(QStringLiteral("No ARQ link"));
    linkDisconnectButton_->setEnabled(false);
    sendButton_->setEnabled(false);
    appendSystemLine(QStringLiteral("ARQ disconnected"));
}

void MainWindow::onDataReceived(const QByteArray &bytes)
{
    const QList<ChatMessage> messages = ChatProtocol::appendAndDecode(chatRxBuffer_, bytes);
    for (const ChatMessage &message : messages)
    {
        const QString speaker = message.from.isEmpty() ? (peerCallsign_.isEmpty() ? QStringLiteral("Remote") : peerCallsign_) : message.from;
        appendTranscript(speaker, message.text);
    }
}

void MainWindow::connectOrDisconnectCat()
{
    if (cat_.isConnected())
    {
        cat_.disconnectRig();
        return;
    }

    bool ok = false;
    const int baud = catBaudCombo_->currentText().trimmed().toInt(&ok);
    const int modelId = catModelCombo_->currentData().toInt();
    if (modelId <= 0)
    {
        QMessageBox::warning(this, QStringLiteral("Radio required"), QStringLiteral("Choose a valid hamlib radio model first."));
        return;
    }

    cat_.connectRig(modelId,
                    catDeviceEdit_->text(),
                    ok ? baud : 0,
                    static_cast<CatSerialLineState>(catRtsCombo_->currentData().toInt()),
                    static_cast<CatSerialLineState>(catDtrCombo_->currentData().toInt()),
                    static_cast<CatPttMethod>(catPttMethodCombo_->currentData().toInt()));
    saveSettings();
}

void MainWindow::updateTncState(bool controlConnected, bool dataConnected)
{
    tncConnectButton_->setText((controlConnected || dataConnected) ? QStringLiteral("Disconnect TNC") : QStringLiteral("Connect TNC"));
    if (controlConnected)
        tncRetryTimer_->stop();
    stationInitButton_->setEnabled(controlConnected);
    beaconSendButton_->setEnabled(controlConnected);
    connectBeaconButton_->setEnabled(controlConnected);
    autoBeaconCheck_->setEnabled(controlConnected);
    if (!controlConnected)
    {
        beaconTimer_->stop();
    }
    else if (autoBeaconCheck_->isChecked() && !beaconTimer_->isActive())
    {
        sendBeacon();
        beaconTimer_->start(beaconIntervalSpin_->value() * 1000);
    }
    tncStatusLabel_->setText(QStringLiteral("Control %1, data %2")
                                 .arg(controlConnected ? QStringLiteral("on") : QStringLiteral("off"),
                                      dataConnected ? QStringLiteral("on") : QStringLiteral("off")));
}

void MainWindow::appendTranscript(const QString &speaker, const QString &text)
{
    transcript_->moveCursor(QTextCursor::End);
    transcript_->insertPlainText(QStringLiteral("[%1] %2: %3\n").arg(utcTimeLabel(), speaker, text));
    transcript_->verticalScrollBar()->setValue(transcript_->verticalScrollBar()->maximum());
}

void MainWindow::appendSystemLine(const QString &text)
{
    transcript_->moveCursor(QTextCursor::End);
    transcript_->insertPlainText(QStringLiteral("[%1] * %2\n").arg(utcTimeLabel(), text));
    transcript_->verticalScrollBar()->setValue(transcript_->verticalScrollBar()->maximum());
}

void MainWindow::updateBeaconRow(const QString &callsign, int bandwidthHz)
{
    for (int row = 0; row < beaconTable_->rowCount(); ++row)
    {
        if (beaconTable_->item(row, 0)->text() == callsign)
        {
            beaconTable_->item(row, 1)->setText(QString::number(bandwidthHz));
            beaconTable_->item(row, 2)->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
            return;
        }
    }

    const int row = beaconTable_->rowCount();
    beaconTable_->insertRow(row);
    beaconTable_->setItem(row, 0, new QTableWidgetItem(callsign));
    beaconTable_->setItem(row, 1, new QTableWidgetItem(QString::number(bandwidthHz)));
    beaconTable_->setItem(row, 2, new QTableWidgetItem(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
}

bool MainWindow::setComboCurrentData(QComboBox *combo, const QVariant &value) const
{
    if (!combo)
        return false;

    const int index = combo->findData(value);
    if (index < 0)
        return false;

    combo->setCurrentIndex(index);
    return true;
}

QString MainWindow::localCallsign() const
{
    return ChatProtocol::normalizeCallsign(callsignEdit_->text());
}

int MainWindow::selectedBandwidth() const
{
    return bandwidthCombo_->currentData().toInt();
}
