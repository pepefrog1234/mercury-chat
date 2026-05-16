#pragma once

#include "CatController.hpp"
#include "ModemProcess.hpp"
#include "TncClient.hpp"

#include <QByteArray>
#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTextEdit;
class QTimer;
class QCloseEvent;
class QVariant;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void connectTnc();
    void initializeStation();
    void sendBeacon();
    void connectSelectedBeacon();
    void startModem();
    void stopModem();
    void retryTncConnection();
    void sendChatMessage();
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
    void appendTranscript(const QString &speaker, const QString &text);
    void appendSystemLine(const QString &text);
    void updateBeaconRow(const QString &callsign, int bandwidthHz);
    bool setComboCurrentData(QComboBox *combo, const QVariant &value) const;
    QString localCallsign() const;
    int selectedBandwidth() const;

    TncClient tnc_;
    CatController cat_;
    ModemProcess modem_;
    QByteArray chatRxBuffer_;
    QString peerCallsign_;
    bool arqConnected_ = false;

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
    QComboBox *bandwidthCombo_ = nullptr;
    QPushButton *tncConnectButton_ = nullptr;
    QPushButton *stationInitButton_ = nullptr;
    QPushButton *linkDisconnectButton_ = nullptr;
    QLabel *tncStatusLabel_ = nullptr;
    QLabel *linkStatusLabel_ = nullptr;
    QLabel *pttStatusLabel_ = nullptr;
    QLabel *bufferStatusLabel_ = nullptr;
    QLabel *snrStatusLabel_ = nullptr;
    QLabel *bitrateStatusLabel_ = nullptr;

    QPushButton *beaconSendButton_ = nullptr;
    QCheckBox *autoBeaconCheck_ = nullptr;
    QSpinBox *beaconIntervalSpin_ = nullptr;
    QTableWidget *beaconTable_ = nullptr;
    QPushButton *connectBeaconButton_ = nullptr;
    QTimer *beaconTimer_ = nullptr;
    QTimer *tncRetryTimer_ = nullptr;
    int tncRetryAttempts_ = 0;

    QTextEdit *transcript_ = nullptr;
    QPlainTextEdit *messageEdit_ = nullptr;
    QPushButton *sendButton_ = nullptr;

    QComboBox *catModelCombo_ = nullptr;
    QLineEdit *catDeviceEdit_ = nullptr;
    QComboBox *catBaudCombo_ = nullptr;
    QComboBox *catRtsCombo_ = nullptr;
    QComboBox *catDtrCombo_ = nullptr;
    QComboBox *catPttMethodCombo_ = nullptr;
    QPushButton *catConnectButton_ = nullptr;
    QPushButton *catReadFreqButton_ = nullptr;
    QLineEdit *catFrequencyEdit_ = nullptr;
    QPushButton *catSetFreqButton_ = nullptr;
    QCheckBox *catPttCheck_ = nullptr;
    QCheckBox *catFollowPttCheck_ = nullptr;
};
