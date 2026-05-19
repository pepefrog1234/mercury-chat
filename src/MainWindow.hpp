#pragma once

#include "CatController.hpp"
#include "ModemProcess.hpp"
#include "SqlLogStore.hpp"
#include "TncClient.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QMainWindow>
#include <QQueue>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSettings;
class QSpinBox;
class QTableWidget;
class QTextEdit;
class QTimer;
class QCloseEvent;
class QEvent;
class QVariant;
struct ChatPartialMessage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &settingsFile = {},
                        const QString &profileName = {},
                        QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void connectTnc();
    void initializeStation();
    void sendBeacon();
    void connectSelectedBeacon();
    void connectEnteredCallsign();
    void startModem();
    void stopModem();
    void retryTncConnection();
    void sendChatMessage();
    void requestLinkDisconnect();
    void updateLinkDuration();
    void onBeaconReceived(const QString &callsign, int bandwidthHz);
    void onLinkConnected(const QString &source, const QString &destination, int bandwidthHz);
    void onLinkDisconnected();
    void onDataReceived(const QByteArray &bytes);
    void connectOrDisconnectCat();
    void updateTncState(bool controlConnected, bool dataConnected);

private:
    void buildUi();
    void wireSignals();
    void loadSettings();
    void saveSettings() const;
    void refreshAudioDeviceLists();
    void initializeDatabase();
    QString defaultDatabasePath() const;
    void recordBeaconEvent(const QString &callsign, int bandwidthHz, double snrDb, bool hasSnr);
    void recordChatMessage(const QString &direction, const QString &speaker, const QString &text);
    void appendTranscript(const QString &speaker, const QString &text);
    void appendIncomingTranscript(const QString &speaker, const QString &text);
    void appendSystemLine(const QString &text);
    void appendStatusLine(const QString &text);
    void showPartialIncoming(const ChatPartialMessage &message);
    void clearPartialIncoming();
    bool receiveInProgress() const;
    bool trySendQueuedMessage();
    void sendQueuedChatMessage(const QString &text);
    void maybeSendTypingIndicator();
    void showRemoteTypingIndicator(const QString &from);
    void clearRemoteTypingIndicator();
    void updateSendControls();
    void insertTranscriptLine(const QString &line, int *blockNumber = nullptr);
    bool replaceTranscriptBlock(int blockNumber, const QString &line);
    void autoInitializeStation();
    void markStationSettingsDirty();
    void beginTransmitProgress(int totalBytes);
    void updateTransmitProgress(int remainingBytes);
    void finishTransmitProgress();
    void finishReceiveProgress();
    void scheduleTransferIdle();
    void setTransferIdle();
    void restartLinkIdleTimer();
    void stopLinkIdleTimer();
    void onLinkIdleTimeout();
    void updateBitrateLabels(int speedLevel, int bitsPerSecond);
    void updateTxBitrateLabels(int speedLevel, int bitsPerSecond);
    void updateBeaconRow(const QString &callsign, int bandwidthHz, double snrDb, bool hasSnr);
    bool applyStationSettings(bool warnIfMissing);
    bool connectCat(bool interactive);
    bool setComboCurrentData(QComboBox *combo, const QVariant &value) const;
    void resetLinkStatus();
    void updateLinkControls();
    QString localCallsign() const;
    int selectedBandwidth() const;
    QSettings *createSettings() const;

    TncClient tnc_;
    CatController cat_;
    ModemProcess modem_;
    SqlLogStore sqlLog_;
    QByteArray chatRxBuffer_;
    QString settingsFile_;
    QString profileName_;
    QString peerCallsign_;
    QString linkSource_;
    QString linkDestination_;
    QDateTime linkConnectedAt_;
    int linkBandwidthHz_ = 0;
    bool arqConnected_ = false;
    bool linkPending_ = false;
    bool beaconCommandAccepted_ = false;
    bool stationSettingsApplied_ = false;
    QString stationAppliedCallsign_;
    int stationAppliedBandwidthHz_ = 0;
    double lastSnrDb_ = 0.0;
    bool hasLastSnrDb_ = false;
    QDateTime lastSnrAt_;
    bool partialRxVisible_ = false;
    int partialRxBlockNumber_ = -1;
    QString partialRxTimeLabel_;
    bool transmitProgressActive_ = false;
    bool transmitProgressSeenBuffer_ = false;
    bool transmitProgressSawPtt_ = false;
    bool receiveProgressActive_ = false;
    int transmitProgressTotalBytes_ = 0;
    int lastBufferBytes_ = 0;
    int currentBitrateLevel_ = 0;
    int currentBitrateBps_ = 0;
    int currentTxBitrateLevel_ = 0;
    int currentTxBitrateBps_ = 0;
    QDateTime lastTypingIndicatorSentAt_;
    QQueue<QString> outboundQueue_;

    QLineEdit *modemPathEdit_ = nullptr;
    QLineEdit *modemArgsEdit_ = nullptr;
    QSpinBox *broadcastPortSpin_ = nullptr;
    QCheckBox *autoStartModemCheck_ = nullptr;
    QPushButton *modemStartButton_ = nullptr;
    QPushButton *modemStopButton_ = nullptr;
    QLabel *modemStatusLabel_ = nullptr;
    QComboBox *soundSystemCombo_ = nullptr;
    QComboBox *inputDeviceCombo_ = nullptr;
    QComboBox *outputDeviceCombo_ = nullptr;
    QComboBox *captureChannelCombo_ = nullptr;

    QLineEdit *hostEdit_ = nullptr;
    QSpinBox *basePortSpin_ = nullptr;
    QLineEdit *callsignEdit_ = nullptr;
    QLineEdit *peerCallsignEdit_ = nullptr;
    QComboBox *bandwidthCombo_ = nullptr;
    QPushButton *tncConnectButton_ = nullptr;
    QPushButton *stationInitButton_ = nullptr;
    QPushButton *connectCallsignButton_ = nullptr;
    QPushButton *linkDisconnectButton_ = nullptr;
    QLabel *tncStatusLabel_ = nullptr;
    QLabel *linkStatusLabel_ = nullptr;
    QLabel *peerStatusLabel_ = nullptr;
    QLabel *linkDurationLabel_ = nullptr;
    QLabel *linkBandwidthLabel_ = nullptr;
    QLabel *pttStatusLabel_ = nullptr;
    QLabel *bufferStatusLabel_ = nullptr;
    QLabel *snrStatusLabel_ = nullptr;
    QLabel *txBitrateStatusLabel_ = nullptr;
    QLabel *bitrateStatusLabel_ = nullptr;

    QPushButton *beaconSendButton_ = nullptr;
    QCheckBox *autoBeaconCheck_ = nullptr;
    QSpinBox *beaconIntervalSpin_ = nullptr;
    QTableWidget *beaconTable_ = nullptr;
    QPushButton *connectBeaconButton_ = nullptr;
    QTimer *beaconTimer_ = nullptr;
    QTimer *tncRetryTimer_ = nullptr;
    QTimer *linkDurationTimer_ = nullptr;
    QTimer *linkIdleTimer_ = nullptr;
    QTimer *transferIdleTimer_ = nullptr;
    QTimer *remoteTypingTimer_ = nullptr;
    int tncRetryAttempts_ = 0;

    QTextEdit *transcript_ = nullptr;
    QPlainTextEdit *messageEdit_ = nullptr;
    QCheckBox *typingIndicatorCheck_ = nullptr;
    QLabel *typingStatusLabel_ = nullptr;
    QLabel *transferStatusLabel_ = nullptr;
    QLabel *transferRateLabel_ = nullptr;
    QProgressBar *transferProgressBar_ = nullptr;
    QPushButton *sendButton_ = nullptr;
    QPlainTextEdit *statusLog_ = nullptr;

    QComboBox *catModelCombo_ = nullptr;
    QLineEdit *catDeviceEdit_ = nullptr;
    QComboBox *catBaudCombo_ = nullptr;
    QComboBox *catRtsCombo_ = nullptr;
    QComboBox *catDtrCombo_ = nullptr;
    QComboBox *catPttMethodCombo_ = nullptr;
    QCheckBox *catAutoConnectCheck_ = nullptr;
    QPushButton *catConnectButton_ = nullptr;
    QPushButton *catReadFreqButton_ = nullptr;
    QLineEdit *catFrequencyEdit_ = nullptr;
    QPushButton *catSetFreqButton_ = nullptr;
    QCheckBox *catPttCheck_ = nullptr;
    QCheckBox *catFollowPttCheck_ = nullptr;
};
