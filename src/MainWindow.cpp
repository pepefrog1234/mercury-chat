#include "MainWindow.hpp"

#include "ChatProtocol.hpp"

#include <QCheckBox>
#include <QCompleter>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QPlainTextEdit>
#include <QProgressBar>
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
#include <QTextBlock>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

namespace
{
constexpr int kLinkIdleDisconnectMs = 5 * 60 * 1000;
constexpr int kTypingIndicatorMinIntervalMs = 15000;
constexpr int kRemoteTypingVisibleMs = 15000;

QString bandwidthLabel(int bandwidthHz)
{
    return QStringLiteral("%1 Hz").arg(bandwidthHz);
}

QString snrLabel(double snrDb, bool hasSnr)
{
    return hasSnr ? QStringLiteral("%1 dB").arg(snrDb, 0, 'f', 1) : QStringLiteral("-");
}

QString bitrateLabel(int speedLevel, int bitsPerSecond)
{
    if (speedLevel <= 0 || bitsPerSecond <= 0)
        return QStringLiteral("-");
    return QStringLiteral("L%1 %2 bps").arg(speedLevel).arg(bitsPerSecond);
}

QString transferRateLabel(int txSpeedLevel, int txBitsPerSecond, int rxSpeedLevel, int rxBitsPerSecond)
{
    return QStringLiteral("TX %1 / RX %2")
        .arg(bitrateLabel(txSpeedLevel, txBitsPerSecond),
             bitrateLabel(rxSpeedLevel, rxBitsPerSecond));
}

QString utcTimeLabel()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

QString durationLabel(qint64 totalSeconds)
{
    if (totalSeconds < 0)
        totalSeconds = 0;

    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds / 60) % 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString transcriptLine(const QString &speaker, const QString &text, const QString &timeLabel = utcTimeLabel())
{
    return QStringLiteral("[%1] %2: %3").arg(timeLabel, speaker, text);
}

QString systemTranscriptLine(const QString &text)
{
    return QStringLiteral("[%1] * %2").arg(utcTimeLabel(), text);
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

MainWindow::MainWindow(const QString &settingsFile, const QString &profileName, QWidget *parent)
    : QMainWindow(parent),
      settingsFile_(settingsFile),
      profileName_(profileName)
{
    buildUi();
    loadSettings();
    wireSignals();
    setWindowTitle(profileName_.isEmpty()
                       ? QStringLiteral("Mercury Chat")
                       : QStringLiteral("Mercury Chat - %1").arg(profileName_));
    if (!settingsFile_.isEmpty())
        appendStatusLine(QStringLiteral("settings: %1").arg(settingsFile_));
    QTimer::singleShot(0, this, [this]() {
        if (catAutoConnectCheck_->isChecked())
            connectCat(false);
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
    peerStatusLabel_ = new QLabel(QStringLiteral("-"), statusGroup);
    linkDurationLabel_ = new QLabel(QStringLiteral("-"), statusGroup);
    linkBandwidthLabel_ = new QLabel(QStringLiteral("-"), statusGroup);
    pttStatusLabel_ = new QLabel(QStringLiteral("PTT off"), statusGroup);
    bufferStatusLabel_ = new QLabel(QStringLiteral("0 bytes"), statusGroup);
    snrStatusLabel_ = new QLabel(QStringLiteral("-"), statusGroup);
    txBitrateStatusLabel_ = new QLabel(QStringLiteral("-"), statusGroup);
    bitrateStatusLabel_ = new QLabel(QStringLiteral("-"), statusGroup);
    statusLayout->addRow(QStringLiteral("Modem"), modemStatusLabel_);
    statusLayout->addRow(QStringLiteral("TNC"), tncStatusLabel_);
    statusLayout->addRow(QStringLiteral("Link"), linkStatusLabel_);
    statusLayout->addRow(QStringLiteral("Peer"), peerStatusLabel_);
    statusLayout->addRow(QStringLiteral("Duration"), linkDurationLabel_);
    statusLayout->addRow(QStringLiteral("Bandwidth"), linkBandwidthLabel_);
    statusLayout->addRow(QStringLiteral("SNR"), snrStatusLabel_);
    statusLayout->addRow(QStringLiteral("TX rate"), txBitrateStatusLabel_);
    statusLayout->addRow(QStringLiteral("RX rate"), bitrateStatusLabel_);
    statusLayout->addRow(QStringLiteral("PTT"), pttStatusLabel_);
    statusLayout->addRow(QStringLiteral("Buffer"), bufferStatusLabel_);

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

    auto *peerConnectRow = new QWidget(beaconGroup);
    auto *peerConnectLayout = new QHBoxLayout(peerConnectRow);
    peerConnectLayout->setContentsMargins(0, 0, 0, 0);
    auto *peerConnectLabel = new QLabel(QStringLiteral("Connect to"), peerConnectRow);
    peerCallsignEdit_ = new QLineEdit(peerConnectRow);
    peerCallsignEdit_->setPlaceholderText(QStringLiteral("Remote callsign"));
    connectCallsignButton_ = new QPushButton(QStringLiteral("Connect"), peerConnectRow);
    connectCallsignButton_->setEnabled(false);
    peerConnectLayout->addWidget(peerConnectLabel);
    peerConnectLayout->addWidget(peerCallsignEdit_, 1);
    peerConnectLayout->addWidget(connectCallsignButton_);

    beaconTable_ = new QTableWidget(0, 4, beaconGroup);
    beaconTable_->setHorizontalHeaderLabels({QStringLiteral("Call"), QStringLiteral("BW"), QStringLiteral("SNR"), QStringLiteral("Last heard")});
    beaconTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    beaconTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    beaconTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    beaconTable_->horizontalHeader()->setStretchLastSection(true);
    beaconTable_->verticalHeader()->setVisible(false);

    connectBeaconButton_ = new QPushButton(QStringLiteral("Connect Selected"), beaconGroup);
    connectBeaconButton_->setEnabled(false);
    beaconLayout->addWidget(beaconControlRow);
    beaconLayout->addWidget(peerConnectRow);
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
    catAutoConnectCheck_ = new QCheckBox(QStringLiteral("Reconnect on app launch"), catGroup);
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
    catLayout->addRow(catAutoConnectCheck_);
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

    leftTabs->addTab(beaconPage, QStringLiteral("Beacons"));
    leftTabs->addTab(modemPage, QStringLiteral("Modem"));
    leftTabs->addTab(stationPage, QStringLiteral("Station"));
    leftTabs->addTab(catPage, QStringLiteral("CAT"));
    leftLayout->addWidget(leftTabs);

    auto *chatPanel = new QWidget(splitter);
    auto *chatPanelLayout = new QVBoxLayout(chatPanel);
    auto *mainTabs = new QTabWidget(chatPanel);

    auto *chatPage = new QWidget(mainTabs);
    auto *chatLayout = new QVBoxLayout(chatPage);
    transcript_ = new QTextEdit(chatPage);
    transcript_->setReadOnly(true);
    transcript_->setAcceptRichText(false);
    messageEdit_ = new QPlainTextEdit(chatPage);
    messageEdit_->setPlaceholderText(QStringLiteral("Type UTF-8 text here"));
    messageEdit_->setFixedHeight(96);
    typingIndicatorCheck_ = new QCheckBox(QStringLiteral("Send typing indicator"), chatPage);
    typingIndicatorCheck_->setChecked(true);
    typingStatusLabel_ = new QLabel(chatPage);
    typingStatusLabel_->setText(QString());
    transferStatusLabel_ = new QLabel(QStringLiteral("Idle"), chatPage);
    transferRateLabel_ = new QLabel(transferRateLabel(0, 0, 0, 0), chatPage);
    transferRateLabel_->setMinimumWidth(220);
    transferRateLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    transferRateLabel_->setToolTip(QStringLiteral("Current payload TXBITRATE and received BITRATE telemetry"));
    transferProgressBar_ = new QProgressBar(chatPage);
    transferProgressBar_->setRange(0, 1);
    transferProgressBar_->setValue(0);
    transferProgressBar_->setTextVisible(true);
    auto *transferRow = new QWidget(chatPage);
    auto *transferLayout = new QHBoxLayout(transferRow);
    transferLayout->setContentsMargins(0, 0, 0, 0);
    transferLayout->addWidget(transferStatusLabel_);
    transferLayout->addWidget(transferProgressBar_, 1);
    transferLayout->addWidget(transferRateLabel_);
    auto *chatOptionRow = new QWidget(chatPage);
    auto *chatOptionLayout = new QHBoxLayout(chatOptionRow);
    chatOptionLayout->setContentsMargins(0, 0, 0, 0);
    chatOptionLayout->addWidget(typingIndicatorCheck_);
    chatOptionLayout->addStretch();
    chatOptionLayout->addWidget(typingStatusLabel_);
    sendButton_ = new QPushButton(QStringLiteral("Send"), chatPage);
    sendButton_->setEnabled(false);

    chatLayout->addWidget(transcript_, 1);
    chatLayout->addWidget(messageEdit_);
    chatLayout->addWidget(chatOptionRow);
    chatLayout->addWidget(transferRow);
    chatLayout->addWidget(sendButton_);

    auto *statusPage = new QWidget(mainTabs);
    auto *statusPageLayout = new QVBoxLayout(statusPage);
    statusLog_ = new QPlainTextEdit(statusPage);
    statusLog_->setReadOnly(true);
    statusLog_->setLineWrapMode(QPlainTextEdit::NoWrap);
    statusPageLayout->addWidget(statusGroup);
    statusPageLayout->addWidget(statusLog_, 1);

    mainTabs->addTab(chatPage, QStringLiteral("Chat"));
    mainTabs->addTab(statusPage, QStringLiteral("Modem Status"));
    chatPanelLayout->addWidget(mainTabs);

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
    linkDurationTimer_ = new QTimer(this);
    linkDurationTimer_->setInterval(1000);
    linkIdleTimer_ = new QTimer(this);
    linkIdleTimer_->setSingleShot(true);
    linkIdleTimer_->setInterval(kLinkIdleDisconnectMs);
    transferIdleTimer_ = new QTimer(this);
    transferIdleTimer_->setSingleShot(true);
    transferIdleTimer_->setInterval(2500);
    remoteTypingTimer_ = new QTimer(this);
    remoteTypingTimer_->setSingleShot(true);
    remoteTypingTimer_->setInterval(kRemoteTypingVisibleMs);
}

void MainWindow::loadSettings()
{
    std::unique_ptr<QSettings> settings(createSettings());

    restoreGeometry(settings->value(QStringLiteral("window/geometry")).toByteArray());

    modemPathEdit_->setText(settings->value(QStringLiteral("modem/executable"), modemPathEdit_->text()).toString());
    modemArgsEdit_->setText(settings->value(QStringLiteral("modem/args")).toString());
    broadcastPortSpin_->setValue(settings->value(QStringLiteral("modem/broadcastPort"), broadcastPortSpin_->value()).toInt());
    autoStartModemCheck_->setChecked(settings->value(QStringLiteral("modem/autoStart"), autoStartModemCheck_->isChecked()).toBool());
    setComboCurrentData(soundSystemCombo_, settings->value(QStringLiteral("audio/soundSystem"), currentComboData(soundSystemCombo_, QStringLiteral("auto"))));
    const QString inputDevice = settings->value(QStringLiteral("audio/inputDevice")).toString();
    if (!setComboCurrentData(inputDeviceCombo_, inputDevice))
        inputDeviceCombo_->setCurrentText(inputDevice);
    const QString outputDevice = settings->value(QStringLiteral("audio/outputDevice")).toString();
    if (!setComboCurrentData(outputDeviceCombo_, outputDevice))
        outputDeviceCombo_->setCurrentText(outputDevice);
    setComboCurrentData(captureChannelCombo_, settings->value(QStringLiteral("audio/captureChannel"), currentComboData(captureChannelCombo_, QStringLiteral("left"))));

    callsignEdit_->setText(settings->value(QStringLiteral("station/callsign")).toString());
    peerCallsignEdit_->setText(settings->value(QStringLiteral("station/peerCallsign")).toString());
    hostEdit_->setText(settings->value(QStringLiteral("tnc/host"), hostEdit_->text()).toString());
    basePortSpin_->setValue(settings->value(QStringLiteral("tnc/basePort"), basePortSpin_->value()).toInt());
    setComboCurrentData(bandwidthCombo_, settings->value(QStringLiteral("tnc/bandwidth"), selectedBandwidth()));
    typingIndicatorCheck_->setChecked(settings->value(QStringLiteral("chat/sendTypingIndicator"), true).toBool());

    autoBeaconCheck_->setChecked(settings->value(QStringLiteral("beacon/auto"), autoBeaconCheck_->isChecked()).toBool());
    beaconIntervalSpin_->setValue(settings->value(QStringLiteral("beacon/intervalSeconds"), beaconIntervalSpin_->value()).toInt());

    setComboCurrentData(catModelCombo_, settings->value(QStringLiteral("cat/modelId"), currentComboData(catModelCombo_, 0)));
    catDeviceEdit_->setText(settings->value(QStringLiteral("cat/device")).toString());
    const QString savedBaud = settings->value(QStringLiteral("cat/baud"), catBaudCombo_->currentText()).toString();
    if (!setComboCurrentData(catBaudCombo_, savedBaud.toInt()))
        catBaudCombo_->setCurrentText(savedBaud);
    setComboCurrentData(catRtsCombo_, settings->value(QStringLiteral("cat/rts"), currentComboData(catRtsCombo_, 0)));
    setComboCurrentData(catDtrCombo_, settings->value(QStringLiteral("cat/dtr"), currentComboData(catDtrCombo_, 0)));
    setComboCurrentData(catPttMethodCombo_, settings->value(QStringLiteral("cat/pttMethod"), currentComboData(catPttMethodCombo_, 0)));
    catAutoConnectCheck_->setChecked(settings->value(QStringLiteral("cat/autoConnect"), catAutoConnectCheck_->isChecked()).toBool());
    catFollowPttCheck_->setChecked(settings->value(QStringLiteral("cat/followModemPtt"), catFollowPttCheck_->isChecked()).toBool());
    catFrequencyEdit_->setText(settings->value(QStringLiteral("cat/frequencyHz"), catFrequencyEdit_->text()).toString());

    if (settings->value(QStringLiteral("window/geometry")).isNull())
        resize(1180, 760);
}

void MainWindow::saveSettings() const
{
    std::unique_ptr<QSettings> settings(createSettings());

    settings->setValue(QStringLiteral("window/geometry"), saveGeometry());

    settings->setValue(QStringLiteral("modem/executable"), modemPathEdit_->text().trimmed());
    settings->setValue(QStringLiteral("modem/args"), modemArgsEdit_->text());
    settings->setValue(QStringLiteral("modem/broadcastPort"), broadcastPortSpin_->value());
    settings->setValue(QStringLiteral("modem/autoStart"), autoStartModemCheck_->isChecked());
    settings->setValue(QStringLiteral("audio/soundSystem"), currentComboData(soundSystemCombo_, QStringLiteral("auto")));
    settings->setValue(QStringLiteral("audio/inputDevice"), comboValue(inputDeviceCombo_));
    settings->setValue(QStringLiteral("audio/outputDevice"), comboValue(outputDeviceCombo_));
    settings->setValue(QStringLiteral("audio/captureChannel"), currentComboData(captureChannelCombo_, QStringLiteral("left")));

    settings->setValue(QStringLiteral("station/callsign"), localCallsign());
    settings->setValue(QStringLiteral("station/peerCallsign"), ChatProtocol::normalizeCallsign(peerCallsignEdit_->text()));
    settings->setValue(QStringLiteral("tnc/host"), hostEdit_->text().trimmed());
    settings->setValue(QStringLiteral("tnc/basePort"), basePortSpin_->value());
    settings->setValue(QStringLiteral("tnc/bandwidth"), selectedBandwidth());
    settings->setValue(QStringLiteral("chat/sendTypingIndicator"), typingIndicatorCheck_->isChecked());

    settings->setValue(QStringLiteral("beacon/auto"), autoBeaconCheck_->isChecked());
    settings->setValue(QStringLiteral("beacon/intervalSeconds"), beaconIntervalSpin_->value());

    settings->setValue(QStringLiteral("cat/modelId"), currentComboData(catModelCombo_, 0));
    settings->setValue(QStringLiteral("cat/device"), catDeviceEdit_->text().trimmed());
    settings->setValue(QStringLiteral("cat/baud"), catBaudCombo_->currentText().trimmed());
    settings->setValue(QStringLiteral("cat/rts"), currentComboData(catRtsCombo_, 0));
    settings->setValue(QStringLiteral("cat/dtr"), currentComboData(catDtrCombo_, 0));
    settings->setValue(QStringLiteral("cat/pttMethod"), currentComboData(catPttMethodCombo_, 0));
    settings->setValue(QStringLiteral("cat/autoConnect"), catAutoConnectCheck_->isChecked());
    settings->setValue(QStringLiteral("cat/followModemPtt"), catFollowPttCheck_->isChecked());
    settings->setValue(QStringLiteral("cat/frequencyHz"), catFrequencyEdit_->text().trimmed());
    settings->sync();
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
        appendStatusLine(QStringLiteral("modem: %1").arg(message));
    });
    connect(&modem_, &ModemProcess::outputLine, this, [this](const QString &line) {
        appendStatusLine(QStringLiteral("modem: %1").arg(line));
    });

    connect(tncConnectButton_, &QPushButton::clicked, this, &MainWindow::connectTnc);
    connect(stationInitButton_, &QPushButton::clicked, this, &MainWindow::initializeStation);
    connect(callsignEdit_, &QLineEdit::editingFinished, this, [this]() {
        callsignEdit_->setText(localCallsign());
        markStationSettingsDirty();
    });
    connect(bandwidthCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        markStationSettingsDirty();
    });
    connect(connectCallsignButton_, &QPushButton::clicked, this, &MainWindow::connectEnteredCallsign);
    connect(peerCallsignEdit_, &QLineEdit::returnPressed, this, &MainWindow::connectEnteredCallsign);
    connect(peerCallsignEdit_, &QLineEdit::editingFinished, this, [this]() {
        saveSettings();
    });
    connect(linkDisconnectButton_, &QPushButton::clicked, this, &MainWindow::requestLinkDisconnect);
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
    connect(linkDurationTimer_, &QTimer::timeout, this, &MainWindow::updateLinkDuration);
    connect(linkIdleTimer_, &QTimer::timeout, this, &MainWindow::onLinkIdleTimeout);
    connect(transferIdleTimer_, &QTimer::timeout, this, &MainWindow::setTransferIdle);
    connect(remoteTypingTimer_, &QTimer::timeout, this, &MainWindow::clearRemoteTypingIndicator);
    connect(typingIndicatorCheck_, &QCheckBox::toggled, this, [this]() {
        saveSettings();
    });
    connect(messageEdit_, &QPlainTextEdit::textChanged, this, [this]() {
        if (arqConnected_)
        {
            restartLinkIdleTimer();
            maybeSendTypingIndicator();
        }
    });

    connect(&tnc_, &TncClient::connectionStateChanged, this, &MainWindow::updateTncState);
    connect(&tnc_, &TncClient::statusMessage, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
        appendStatusLine(QStringLiteral("TNC: %1").arg(message));
    });
    connect(&tnc_, &TncClient::controlLineReceived, this, [this](const QString &line) {
        appendStatusLine(QStringLiteral("TNC: %1").arg(line));
    });
    connect(&tnc_, &TncClient::commandCompleted, this, [this](const QString &command, const QString &response) {
        const QString label = command.isEmpty() ? QStringLiteral("(unknown command)") : command;
        appendStatusLine(QStringLiteral("TNC command %1: %2").arg(label, response));

        if (command.startsWith(QStringLiteral("CQFRAME ")))
        {
            if (response == QLatin1String("OK"))
            {
                beaconCommandAccepted_ = true;
                appendSystemLine(QStringLiteral("Beacon accepted by modem"));
            }
            else
            {
                beaconCommandAccepted_ = false;
                appendSystemLine(QStringLiteral("Beacon rejected by modem: %1").arg(command));
            }
        }
    });
    connect(&tnc_, &TncClient::cqFrameReceived, this, &MainWindow::onBeaconReceived);
    connect(&tnc_, &TncClient::linkConnected, this, &MainWindow::onLinkConnected);
    connect(&tnc_, &TncClient::linkDisconnected, this, &MainWindow::onLinkDisconnected);
    connect(&tnc_, &TncClient::pendingChanged, this, [this](bool pending) {
        linkPending_ = pending;
        if (!arqConnected_)
            linkStatusLabel_->setText(pending ? QStringLiteral("Pending") : QStringLiteral("No ARQ link"));
        if (beaconCommandAccepted_)
        {
            appendSystemLine(pending ? QStringLiteral("Beacon transmission started")
                                     : QStringLiteral("Beacon transmission completed"));
            if (!pending)
                beaconCommandAccepted_ = false;
        }
        updateLinkControls();
    });
    connect(&tnc_, &TncClient::pttChanged, this, [this](bool enabled) {
        pttStatusLabel_->setText(enabled ? QStringLiteral("PTT on") : QStringLiteral("PTT off"));
        if (catFollowPttCheck_->isChecked() && cat_.isConnected())
            cat_.setPtt(enabled);
        if (transmitProgressActive_)
        {
            if (enabled)
                transmitProgressSawPtt_ = true;
            else
                tnc_.queryBuffer();
        }
    });
    connect(&tnc_, &TncClient::bufferUpdated, this, [this](int bytes) {
        lastBufferBytes_ = bytes;
        bufferStatusLabel_->setText(QStringLiteral("%1 bytes").arg(bytes));
        updateTransmitProgress(bytes);
        if (!transmitProgressActive_ && bytes == 0)
            trySendQueuedMessage();
        else
            updateSendControls();
    });
    connect(&tnc_, &TncClient::snrUpdated, this, [this](double snr) {
        lastSnrDb_ = snr;
        hasLastSnrDb_ = true;
        lastSnrAt_ = QDateTime::currentDateTime();
        snrStatusLabel_->setText(QStringLiteral("%1 dB").arg(snr, 0, 'f', 1));
    });
    connect(&tnc_, &TncClient::bitrateUpdated, this, [this](int level, int bps) {
        updateBitrateLabels(level, bps);
    });
    connect(&tnc_, &TncClient::txBitrateUpdated, this, [this](int level, int bps) {
        updateTxBitrateLabels(level, bps);
    });
    connect(&tnc_, &TncClient::dataReceived, this, &MainWindow::onDataReceived);

    connect(catConnectButton_, &QPushButton::clicked, this, &MainWindow::connectOrDisconnectCat);
    connect(catReadFreqButton_, &QPushButton::clicked, &cat_, &CatController::refreshFrequency);
    connect(catSetFreqButton_, &QPushButton::clicked, this, [this]() {
        cat_.setFrequencyHz(catFrequencyEdit_->text().trimmed().toLongLong());
        saveSettings();
    });
    connect(catAutoConnectCheck_, &QCheckBox::toggled, this, [this]() {
        saveSettings();
    });
    connect(catFollowPttCheck_, &QCheckBox::toggled, this, [this]() {
        saveSettings();
    });
    connect(catFrequencyEdit_, &QLineEdit::editingFinished, this, [this]() {
        saveSettings();
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
        appendStatusLine(QStringLiteral("CAT: %1").arg(message));
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
    resetLinkStatus();
    modem_.stop();
}

void MainWindow::connectTnc()
{
    saveSettings();

    if (tnc_.isControlConnected() || tnc_.isDataConnected())
    {
        tncRetryTimer_->stop();
        tnc_.disconnectFromModem();
        resetLinkStatus();
        return;
    }

    tncRetryAttempts_ = 0;
    tncRetryTimer_->start();
    tnc_.connectToModem(hostEdit_->text().trimmed(), static_cast<quint16>(basePortSpin_->value()));
}

void MainWindow::requestLinkDisconnect()
{
    if (!tnc_.isControlConnected())
        return;

    tnc_.disconnectLink();
    linkStatusLabel_->setText(arqConnected_ ? QStringLiteral("Disconnecting") : QStringLiteral("No ARQ link"));
    appendSystemLine(QStringLiteral("Disconnect requested"));
    updateLinkControls();
}

void MainWindow::updateLinkDuration()
{
    if (!arqConnected_ || !linkConnectedAt_.isValid())
    {
        linkDurationLabel_->setText(QStringLiteral("-"));
        return;
    }

    linkDurationLabel_->setText(durationLabel(linkConnectedAt_.secsTo(QDateTime::currentDateTime())));
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
        appendStatusLine(QStringLiteral("TNC connection retry stopped; modem control port is still unavailable."));
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
            appendStatusLine(QStringLiteral("Station settings not applied: TNC control port is not connected."));
        return false;
    }

    const int bandwidthHz = selectedBandwidth();
    tnc_.initializeStation(callsign, bandwidthHz);
    stationSettingsApplied_ = true;
    stationAppliedCallsign_ = callsign;
    stationAppliedBandwidthHz_ = bandwidthHz;
    saveSettings();
    return true;
}

void MainWindow::initializeStation()
{
    if (applyStationSettings(true))
        appendStatusLine(QStringLiteral("Initialized %1 at %2").arg(localCallsign(), bandwidthLabel(selectedBandwidth())));
}

void MainWindow::sendBeacon()
{
    if (arqConnected_)
    {
        beaconTimer_->stop();
        appendSystemLine(QStringLiteral("Beacon not sent: ARQ link is active."));
        updateLinkControls();
        return;
    }

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
    appendSystemLine(QStringLiteral("Beacon requested as %1 %2").arg(callsign, bandwidthLabel(selectedBandwidth())));
}

void MainWindow::connectSelectedBeacon()
{
    if (arqConnected_)
    {
        requestLinkDisconnect();
        return;
    }

    const QList<QTableWidgetItem *> selected = beaconTable_->selectedItems();
    if (selected.isEmpty())
        return;

    const int row = selected.first()->row();
    const QString target = beaconTable_->item(row, 0)->text();
    peerCallsignEdit_->setText(target);
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

    peerCallsign_ = target;
    peerStatusLabel_->setText(peerCallsign_);
    linkStatusLabel_->setText(QStringLiteral("Calling %1").arg(target));
    updateLinkControls();

    tnc_.connectPeer(callsign, target);
    appendSystemLine(QStringLiteral("Connecting %1 -> %2").arg(callsign, target));
}

void MainWindow::connectEnteredCallsign()
{
    if (arqConnected_)
    {
        requestLinkDisconnect();
        return;
    }

    const QString target = ChatProtocol::normalizeCallsign(peerCallsignEdit_->text());
    peerCallsignEdit_->setText(target);
    saveSettings();

    const QString callsign = localCallsign();
    if (callsign.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("Callsign required"), QStringLiteral("Set a valid local callsign before connecting."));
        return;
    }

    if (target.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("Remote callsign required"), QStringLiteral("Enter the remote callsign before connecting."));
        return;
    }

    if (target == callsign)
    {
        QMessageBox::warning(this, QStringLiteral("Remote callsign required"), QStringLiteral("Enter a callsign different from your local callsign."));
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

    peerCallsign_ = target;
    peerStatusLabel_->setText(peerCallsign_);
    linkStatusLabel_->setText(QStringLiteral("Calling %1").arg(target));
    updateLinkControls();

    tnc_.connectPeer(callsign, target);
    appendSystemLine(QStringLiteral("Connecting %1 -> %2").arg(callsign, target));
}

void MainWindow::sendChatMessage()
{
    const QString text = messageEdit_->toPlainText();
    if (text.trimmed().isEmpty())
        return;

    outboundQueue_.enqueue(text);
    messageEdit_->clear();
    restartLinkIdleTimer();
    trySendQueuedMessage();
    updateSendControls();
}

void MainWindow::onBeaconReceived(const QString &callsign, int bandwidthHz)
{
    if (callsign.isEmpty() || callsign == localCallsign())
        return;

    const bool hasRecentSnr = hasLastSnrDb_ &&
                              lastSnrAt_.isValid() &&
                              lastSnrAt_.msecsTo(QDateTime::currentDateTime()) <= 15000;
    updateBeaconRow(callsign, bandwidthHz, lastSnrDb_, hasRecentSnr);
    appendSystemLine(QStringLiteral("Beacon heard from %1 %2, SNR %3")
                         .arg(callsign, bandwidthLabel(bandwidthHz), snrLabel(lastSnrDb_, hasRecentSnr)));
}

void MainWindow::onLinkConnected(const QString &source, const QString &destination, int bandwidthHz)
{
    arqConnected_ = true;
    linkPending_ = false;
    lastTypingIndicatorSentAt_ = {};
    clearRemoteTypingIndicator();
    linkSource_ = source;
    linkDestination_ = destination;
    linkBandwidthHz_ = bandwidthHz;
    linkConnectedAt_ = QDateTime::currentDateTime();

    const QString local = localCallsign();
    if (source == local)
        peerCallsign_ = destination;
    else if (destination == local)
        peerCallsign_ = source;
    else
        peerCallsign_ = source.isEmpty() ? destination : source;

    peerStatusLabel_->setText(peerCallsign_.isEmpty() ? QStringLiteral("-") : peerCallsign_);
    if (!peerCallsign_.isEmpty())
        peerCallsignEdit_->setText(peerCallsign_);
    linkStatusLabel_->setText(QStringLiteral("Connected (%1 -> %2)").arg(source, destination));
    linkBandwidthLabel_->setText(bandwidthLabel(bandwidthHz));
    snrStatusLabel_->setText(QStringLiteral("Waiting"));
    updateTxBitrateLabels(0, 0);
    updateBitrateLabels(0, 0);
    updateLinkDuration();
    linkDurationTimer_->start();
    restartLinkIdleTimer();
    beaconTimer_->stop();
    updateLinkControls();
    appendSystemLine(QStringLiteral("ARQ connected: %1 -> %2, %3").arg(source, destination, bandwidthLabel(bandwidthHz)));
}

void MainWindow::onLinkDisconnected()
{
    const QString oldPeer = peerCallsign_;
    resetLinkStatus();
    appendSystemLine(QStringLiteral("ARQ disconnected"));
    if (!oldPeer.isEmpty())
        appendStatusLine(QStringLiteral("Link with %1 closed").arg(oldPeer));
}

void MainWindow::onDataReceived(const QByteArray &bytes)
{
    restartLinkIdleTimer();

    const QList<ChatMessage> messages = ChatProtocol::appendAndDecode(chatRxBuffer_, bytes);
    bool decodedContentMessages = false;
    for (const ChatMessage &message : messages)
    {
        const QString speaker = message.from.isEmpty() ? (peerCallsign_.isEmpty() ? QStringLiteral("Remote") : peerCallsign_) : message.from;
        if (message.kind == ChatMessage::Kind::Typing)
        {
            showRemoteTypingIndicator(speaker);
            continue;
        }

        clearRemoteTypingIndicator();
        appendIncomingTranscript(speaker, message.text);
        decodedContentMessages = true;
    }

    const ChatPartialMessage partial = ChatProtocol::previewIncompleteMessage(chatRxBuffer_);
    if (partial.active || partial.totalBytesKnown)
        showPartialIncoming(partial);
    else
    {
        clearPartialIncoming();
        if (decodedContentMessages && chatRxBuffer_.isEmpty())
            finishReceiveProgress();
        else
            trySendQueuedMessage();
    }
}

void MainWindow::connectOrDisconnectCat()
{
    if (cat_.isConnected())
    {
        cat_.disconnectRig();
        saveSettings();
        return;
    }

    connectCat(true);
}

bool MainWindow::connectCat(bool interactive)
{
    bool ok = false;
    const int baud = catBaudCombo_->currentText().trimmed().toInt(&ok);
    const int modelId = catModelCombo_->currentData().toInt();
    if (modelId <= 0)
    {
        if (interactive)
            QMessageBox::warning(this, QStringLiteral("Radio required"), QStringLiteral("Choose a valid hamlib radio model first."));
        else
            appendStatusLine(QStringLiteral("CAT auto-connect skipped: choose a valid hamlib radio model first."));
        return false;
    }

    cat_.connectRig(modelId,
                    catDeviceEdit_->text(),
                    ok ? baud : 0,
                    static_cast<CatSerialLineState>(catRtsCombo_->currentData().toInt()),
                    static_cast<CatSerialLineState>(catDtrCombo_->currentData().toInt()),
                    static_cast<CatPttMethod>(catPttMethodCombo_->currentData().toInt()));
    saveSettings();
    return cat_.isConnected();
}

void MainWindow::resetLinkStatus()
{
    arqConnected_ = false;
    linkPending_ = false;
    peerCallsign_.clear();
    linkSource_.clear();
    linkDestination_.clear();
    linkConnectedAt_ = {};
    linkBandwidthHz_ = 0;
    linkDurationTimer_->stop();

    linkStatusLabel_->setText(QStringLiteral("No ARQ link"));
    peerStatusLabel_->setText(QStringLiteral("-"));
    linkDurationLabel_->setText(QStringLiteral("-"));
    linkBandwidthLabel_->setText(QStringLiteral("-"));
    snrStatusLabel_->setText(QStringLiteral("-"));
    updateTxBitrateLabels(0, 0);
    updateBitrateLabels(0, 0);
    bufferStatusLabel_->setText(QStringLiteral("0 bytes"));
    lastBufferBytes_ = 0;
    lastTypingIndicatorSentAt_ = {};
    chatRxBuffer_.clear();
    clearPartialIncoming();
    clearRemoteTypingIndicator();
    outboundQueue_.clear();
    stopLinkIdleTimer();
    setTransferIdle();
    updateLinkControls();
}

void MainWindow::updateLinkControls()
{
    const bool controlConnected = tnc_.isControlConnected();
    const bool beaconAllowed = controlConnected && !arqConnected_;

    linkDisconnectButton_->setEnabled(controlConnected && arqConnected_);
    updateSendControls();
    beaconSendButton_->setEnabled(beaconAllowed);
    autoBeaconCheck_->setEnabled(beaconAllowed);
    beaconIntervalSpin_->setEnabled(beaconAllowed);

    if (!beaconAllowed)
    {
        beaconTimer_->stop();
    }
    else if (autoBeaconCheck_->isChecked() && !beaconTimer_->isActive())
    {
        beaconTimer_->start(beaconIntervalSpin_->value() * 1000);
    }

    connectCallsignButton_->setText(arqConnected_ ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
    connectCallsignButton_->setEnabled(controlConnected);

    connectBeaconButton_->setText(arqConnected_ ? QStringLiteral("Disconnect Link") : QStringLiteral("Connect Selected"));
    connectBeaconButton_->setEnabled(controlConnected);
}

void MainWindow::updateTncState(bool controlConnected, bool dataConnected)
{
    tncConnectButton_->setText((controlConnected || dataConnected) ? QStringLiteral("Disconnect TNC") : QStringLiteral("Connect TNC"));
    if (controlConnected)
    {
        tncRetryTimer_->stop();
        autoInitializeStation();
    }
    stationInitButton_->setEnabled(controlConnected);
    beaconSendButton_->setEnabled(controlConnected);
    connectBeaconButton_->setEnabled(controlConnected);
    connectCallsignButton_->setEnabled(controlConnected);
    autoBeaconCheck_->setEnabled(controlConnected);
    if (!controlConnected)
    {
        stationSettingsApplied_ = false;
        stationAppliedCallsign_.clear();
        stationAppliedBandwidthHz_ = 0;
        hasLastSnrDb_ = false;
        lastSnrDb_ = 0.0;
        lastSnrAt_ = {};
        beaconTimer_->stop();
        if (arqConnected_ || linkPending_ || linkConnectedAt_.isValid())
            resetLinkStatus();
    }
    else if (autoBeaconCheck_->isChecked() && !beaconTimer_->isActive())
    {
        sendBeacon();
        beaconTimer_->start(beaconIntervalSpin_->value() * 1000);
    }
    tncStatusLabel_->setText(QStringLiteral("Control %1, data %2")
                                 .arg(controlConnected ? QStringLiteral("on") : QStringLiteral("off"),
                                      dataConnected ? QStringLiteral("on") : QStringLiteral("off")));
    updateLinkControls();
}

void MainWindow::appendTranscript(const QString &speaker, const QString &text)
{
    insertTranscriptLine(transcriptLine(speaker, text));
}

void MainWindow::appendIncomingTranscript(const QString &speaker, const QString &text)
{
    const QString line = transcriptLine(speaker, text, partialRxVisible_ ? partialRxTimeLabel_ : utcTimeLabel());
    if (partialRxVisible_ && replaceTranscriptBlock(partialRxBlockNumber_, line))
    {
        partialRxVisible_ = false;
        partialRxBlockNumber_ = -1;
        partialRxTimeLabel_.clear();
        return;
    }

    clearPartialIncoming();
    insertTranscriptLine(line);
}

void MainWindow::appendSystemLine(const QString &text)
{
    insertTranscriptLine(systemTranscriptLine(text));
}

void MainWindow::appendStatusLine(const QString &text)
{
    statusLog_->appendPlainText(QStringLiteral("[%1] %2").arg(utcTimeLabel(), text));
    statusLog_->verticalScrollBar()->setValue(statusLog_->verticalScrollBar()->maximum());
}

void MainWindow::autoInitializeStation()
{
    if (!tnc_.isControlConnected())
        return;

    const QString callsign = localCallsign();
    const int bandwidthHz = selectedBandwidth();
    if (callsign.isEmpty() || bandwidthHz <= 0)
        return;

    if (stationSettingsApplied_ &&
        stationAppliedCallsign_ == callsign &&
        stationAppliedBandwidthHz_ == bandwidthHz)
    {
        return;
    }

    tnc_.initializeStation(callsign, bandwidthHz);
    stationSettingsApplied_ = true;
    stationAppliedCallsign_ = callsign;
    stationAppliedBandwidthHz_ = bandwidthHz;
    appendStatusLine(QStringLiteral("Auto-initialized %1 at %2").arg(callsign, bandwidthLabel(bandwidthHz)));
}

void MainWindow::markStationSettingsDirty()
{
    stationSettingsApplied_ = false;
    stationAppliedCallsign_.clear();
    stationAppliedBandwidthHz_ = 0;
    saveSettings();
    autoInitializeStation();
}

void MainWindow::showPartialIncoming(const ChatPartialMessage &message)
{
    if (!transmitProgressActive_)
    {
        transferIdleTimer_->stop();
        receiveProgressActive_ = true;
        if (message.totalBytesKnown && message.totalBytes > 0)
        {
            transferProgressBar_->setRange(0, static_cast<int>(message.totalBytes));
            transferProgressBar_->setValue(static_cast<int>(qMin(message.bytesBuffered, message.totalBytes)));
            transferStatusLabel_->setText(QStringLiteral("RX %1/%2 bytes")
                                              .arg(message.bytesBuffered)
                                              .arg(message.totalBytes));
        }
        else if (message.active)
        {
            transferProgressBar_->setRange(0, 0);
            transferStatusLabel_->setText(QStringLiteral("RX %1 bytes").arg(message.bytesBuffered));
        }
    }
    updateSendControls();

    if (!message.active)
        return;

    const QString speaker = message.from.isEmpty()
                                ? (peerCallsign_.isEmpty() ? QStringLiteral("Remote") : peerCallsign_)
                                : message.from;

    if (!partialRxVisible_)
        partialRxTimeLabel_ = utcTimeLabel();

    const QString line = transcriptLine(speaker, message.text, partialRxTimeLabel_);
    if (partialRxVisible_)
    {
        if (!replaceTranscriptBlock(partialRxBlockNumber_, line))
        {
            partialRxVisible_ = false;
            partialRxBlockNumber_ = -1;
        }
    }

    if (!partialRxVisible_)
    {
        insertTranscriptLine(line, &partialRxBlockNumber_);
        partialRxVisible_ = true;
    }

}

void MainWindow::clearPartialIncoming()
{
    partialRxVisible_ = false;
    partialRxBlockNumber_ = -1;
    partialRxTimeLabel_.clear();
}

bool MainWindow::receiveInProgress() const
{
    return receiveProgressActive_ || partialRxVisible_ || !chatRxBuffer_.isEmpty();
}

bool MainWindow::trySendQueuedMessage()
{
    if (outboundQueue_.isEmpty())
    {
        updateSendControls();
        return false;
    }

    if (!arqConnected_ || !tnc_.isDataConnected())
    {
        updateSendControls();
        return false;
    }

    if (transmitProgressActive_ || receiveInProgress())
    {
        updateSendControls();
        return false;
    }

    if (lastBufferBytes_ > 0)
    {
        tnc_.queryBuffer();
        transferProgressBar_->setRange(0, 1);
        transferProgressBar_->setValue(0);
        transferStatusLabel_->setText(QStringLiteral("Queued: %1 message(s)").arg(outboundQueue_.size()));
        updateSendControls();
        return false;
    }

    sendQueuedChatMessage(outboundQueue_.dequeue());
    updateSendControls();
    return true;
}

void MainWindow::sendQueuedChatMessage(const QString &text)
{
    restartLinkIdleTimer();

    const QString callsign = localCallsign();
    const QByteArray payload = ChatProtocol::encodeTextMessage(callsign, text);
    beginTransmitProgress(payload.size());
    tnc_.sendPayload(payload);
    tnc_.queryBuffer();
    appendTranscript(callsign, text);
}

void MainWindow::maybeSendTypingIndicator()
{
    if (!typingIndicatorCheck_->isChecked() || !arqConnected_ || !tnc_.isDataConnected())
        return;

    if (messageEdit_->toPlainText().trimmed().isEmpty())
        return;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (lastTypingIndicatorSentAt_.isValid() &&
        lastTypingIndicatorSentAt_.msecsTo(now) < kTypingIndicatorMinIntervalMs)
    {
        return;
    }

    lastTypingIndicatorSentAt_ = now;
    tnc_.sendPayload(ChatProtocol::encodeTypingNotification(localCallsign()));
    tnc_.queryBuffer();
}

void MainWindow::showRemoteTypingIndicator(const QString &from)
{
    const QString speaker = from.isEmpty()
                                ? (peerCallsign_.isEmpty() ? QStringLiteral("Remote") : peerCallsign_)
                                : from;
    typingStatusLabel_->setText(QStringLiteral("%1 is typing...").arg(speaker));
    remoteTypingTimer_->start();
}

void MainWindow::clearRemoteTypingIndicator()
{
    typingStatusLabel_->clear();
    remoteTypingTimer_->stop();
}

void MainWindow::updateSendControls()
{
    const bool connected = tnc_.isDataConnected() && arqConnected_;
    sendButton_->setEnabled(connected);

    if (!connected)
    {
        sendButton_->setText(QStringLiteral("Send"));
        return;
    }

    if (!outboundQueue_.isEmpty())
    {
        sendButton_->setText(QStringLiteral("Queue (%1)").arg(outboundQueue_.size()));
        return;
    }

    sendButton_->setText((transmitProgressActive_ || receiveInProgress())
                             ? QStringLiteral("Queue")
                             : QStringLiteral("Send"));
}

void MainWindow::insertTranscriptLine(const QString &line, int *blockNumber)
{
    transcript_->moveCursor(QTextCursor::End);
    if (blockNumber)
        *blockNumber = transcript_->document()->blockCount() - 1;
    transcript_->insertPlainText(line + QLatin1Char('\n'));
    transcript_->verticalScrollBar()->setValue(transcript_->verticalScrollBar()->maximum());
}

bool MainWindow::replaceTranscriptBlock(int blockNumber, const QString &line)
{
    if (blockNumber < 0)
        return false;

    const QTextBlock block = transcript_->document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(line);
    transcript_->verticalScrollBar()->setValue(transcript_->verticalScrollBar()->maximum());
    return true;
}

void MainWindow::beginTransmitProgress(int totalBytes)
{
    if (totalBytes <= 0)
        return;

    transmitProgressActive_ = true;
    transmitProgressSeenBuffer_ = false;
    transmitProgressSawPtt_ = false;
    transmitProgressTotalBytes_ = qMax(lastBufferBytes_, 0) + totalBytes;
    if (lastBufferBytes_ > 0)
        transmitProgressSeenBuffer_ = true;
    receiveProgressActive_ = false;
    transferIdleTimer_->stop();
    transferProgressBar_->setRange(0, transmitProgressTotalBytes_);
    transferProgressBar_->setValue(0);
    transferStatusLabel_->setText(QStringLiteral("TX queued: %1 bytes").arg(transmitProgressTotalBytes_));
    updateSendControls();
}

void MainWindow::updateTransmitProgress(int remainingBytes)
{
    if (!transmitProgressActive_ || transmitProgressTotalBytes_ <= 0)
        return;

    if (remainingBytes > transmitProgressTotalBytes_)
        transmitProgressTotalBytes_ = remainingBytes;

    const int clampedRemaining = qBound(0, remainingBytes, transmitProgressTotalBytes_);
    if (remainingBytes > 0)
        transmitProgressSeenBuffer_ = true;
    if (clampedRemaining == 0 && !transmitProgressSeenBuffer_ && !transmitProgressSawPtt_)
        return;

    const int sentBytes = transmitProgressTotalBytes_ - clampedRemaining;
    transferProgressBar_->setRange(0, transmitProgressTotalBytes_);
    transferProgressBar_->setValue(sentBytes);

    if (clampedRemaining == 0)
    {
        finishTransmitProgress();
        return;
    }

    const int percent = (sentBytes * 100) / transmitProgressTotalBytes_;
    transferStatusLabel_->setText(QStringLiteral("TX %1% (%2/%3 bytes)")
                                      .arg(percent)
                                      .arg(sentBytes)
                                      .arg(transmitProgressTotalBytes_));
}

void MainWindow::finishTransmitProgress()
{
    const int totalBytes = qMax(transmitProgressTotalBytes_, 1);
    transferProgressBar_->setRange(0, totalBytes);
    transferProgressBar_->setValue(totalBytes);
    transferStatusLabel_->setText(QStringLiteral("TX complete"));
    transmitProgressActive_ = false;
    transmitProgressSeenBuffer_ = false;
    transmitProgressSawPtt_ = false;
    transmitProgressTotalBytes_ = 0;

    const ChatPartialMessage partial = ChatProtocol::previewIncompleteMessage(chatRxBuffer_);
    if (partial.active || partial.totalBytesKnown)
        showPartialIncoming(partial);

    if (trySendQueuedMessage())
        return;
    if (receiveInProgress() || !outboundQueue_.isEmpty())
        return;
    scheduleTransferIdle();
}

void MainWindow::finishReceiveProgress()
{
    if (transmitProgressActive_)
        return;

    receiveProgressActive_ = false;
    transferProgressBar_->setRange(0, 1);
    transferProgressBar_->setValue(1);
    transferStatusLabel_->setText(QStringLiteral("RX complete"));
    if (trySendQueuedMessage())
        return;
    if (!outboundQueue_.isEmpty())
        return;
    scheduleTransferIdle();
}

void MainWindow::scheduleTransferIdle()
{
    transferIdleTimer_->start();
}

void MainWindow::setTransferIdle()
{
    transmitProgressActive_ = false;
    transmitProgressSeenBuffer_ = false;
    transmitProgressSawPtt_ = false;
    receiveProgressActive_ = false;
    transmitProgressTotalBytes_ = 0;
    transferIdleTimer_->stop();
    transferProgressBar_->setRange(0, 1);
    transferProgressBar_->setValue(0);
    if (outboundQueue_.isEmpty())
        transferStatusLabel_->setText(QStringLiteral("Idle"));
    else
        transferStatusLabel_->setText(QStringLiteral("Queued: %1 message(s)").arg(outboundQueue_.size()));
    updateSendControls();
}

void MainWindow::restartLinkIdleTimer()
{
    if (!arqConnected_ || !linkIdleTimer_)
        return;

    linkIdleTimer_->start();
}

void MainWindow::stopLinkIdleTimer()
{
    if (linkIdleTimer_)
        linkIdleTimer_->stop();
}

void MainWindow::onLinkIdleTimeout()
{
    if (!arqConnected_)
        return;

    if (transmitProgressActive_ || receiveInProgress() || lastBufferBytes_ > 0)
    {
        restartLinkIdleTimer();
        return;
    }

    if (!outboundQueue_.isEmpty())
    {
        if (trySendQueuedMessage())
            return;
        restartLinkIdleTimer();
        return;
    }

    appendSystemLine(QStringLiteral("Link idle for 5 minutes; disconnecting"));
    tnc_.disconnectLink();
}

void MainWindow::updateBitrateLabels(int speedLevel, int bitsPerSecond)
{
    currentBitrateLevel_ = speedLevel;
    currentBitrateBps_ = bitsPerSecond;

    bitrateStatusLabel_->setText(bitrateLabel(currentBitrateLevel_, currentBitrateBps_));
    transferRateLabel_->setText(transferRateLabel(currentTxBitrateLevel_,
                                                  currentTxBitrateBps_,
                                                  currentBitrateLevel_,
                                                  currentBitrateBps_));
}

void MainWindow::updateTxBitrateLabels(int speedLevel, int bitsPerSecond)
{
    currentTxBitrateLevel_ = speedLevel;
    currentTxBitrateBps_ = bitsPerSecond;

    txBitrateStatusLabel_->setText(bitrateLabel(currentTxBitrateLevel_, currentTxBitrateBps_));
    transferRateLabel_->setText(transferRateLabel(currentTxBitrateLevel_,
                                                  currentTxBitrateBps_,
                                                  currentBitrateLevel_,
                                                  currentBitrateBps_));
}

void MainWindow::updateBeaconRow(const QString &callsign, int bandwidthHz, double snrDb, bool hasSnr)
{
    for (int row = 0; row < beaconTable_->rowCount(); ++row)
    {
        if (beaconTable_->item(row, 0)->text() == callsign)
        {
            beaconTable_->item(row, 1)->setText(QString::number(bandwidthHz));
            beaconTable_->item(row, 2)->setText(snrLabel(snrDb, hasSnr));
            beaconTable_->item(row, 3)->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
            return;
        }
    }

    const int row = beaconTable_->rowCount();
    beaconTable_->insertRow(row);
    beaconTable_->setItem(row, 0, new QTableWidgetItem(callsign));
    beaconTable_->setItem(row, 1, new QTableWidgetItem(QString::number(bandwidthHz)));
    beaconTable_->setItem(row, 2, new QTableWidgetItem(snrLabel(snrDb, hasSnr)));
    beaconTable_->setItem(row, 3, new QTableWidgetItem(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
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

QSettings *MainWindow::createSettings() const
{
    if (!settingsFile_.isEmpty())
        return new QSettings(settingsFile_, QSettings::IniFormat);

    return new QSettings();
}

QString MainWindow::localCallsign() const
{
    return ChatProtocol::normalizeCallsign(callsignEdit_->text());
}

int MainWindow::selectedBandwidth() const
{
    return bandwidthCombo_->currentData().toInt();
}
