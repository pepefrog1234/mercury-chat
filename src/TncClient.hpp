#pragma once

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QTcpSocket>

class TncClient : public QObject
{
    Q_OBJECT

public:
    explicit TncClient(QObject *parent = nullptr);

    bool isControlConnected() const;
    bool isDataConnected() const;

public slots:
    void connectToModem(const QString &host, quint16 basePort);
    void disconnectFromModem();
    void initializeStation(const QString &callsign, int bandwidthHz);
    void sendCommand(const QString &command);
    void sendCqFrame(const QString &callsign, int bandwidthHz);
    void connectPeer(const QString &myCallsign, const QString &theirCallsign);
    void disconnectLink();
    void abortLink();
    void queryBuffer();
    void sendPayload(const QByteArray &payload);

signals:
    void connectionStateChanged(bool controlConnected, bool dataConnected);
    void statusMessage(const QString &message);
    void controlLineReceived(const QString &line);
    void commandResponse(const QString &line);
    void commandCompleted(const QString &command, const QString &response);
    void cqFrameReceived(const QString &callsign, int bandwidthHz);
    void linkConnected(const QString &source, const QString &destination, int bandwidthHz);
    void linkDisconnected();
    void pendingChanged(bool pending);
    void pttChanged(bool enabled);
    void bufferUpdated(int bytes);
    void snrUpdated(double snrDb);
    void bitrateUpdated(int speedLevel, int bitsPerSecond);
    void txBitrateUpdated(int speedLevel, int bitsPerSecond);
    void dataReceived(const QByteArray &bytes);

private slots:
    void onControlReadyRead();
    void onDataReadyRead();
    void emitSocketState();

private:
    void parseControlLine(const QString &line);

    QTcpSocket controlSocket_;
    QTcpSocket dataSocket_;
    QByteArray controlBuffer_;
    QQueue<QString> pendingCommands_;
};
