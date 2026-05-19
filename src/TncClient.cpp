#include "TncClient.hpp"

#include "ChatProtocol.hpp"

#include <QAbstractSocket>
#include <QRegularExpression>

TncClient::TncClient(QObject *parent)
    : QObject(parent),
      controlSocket_(this),
      dataSocket_(this)
{
    connect(&controlSocket_, &QTcpSocket::readyRead, this, &TncClient::onControlReadyRead);
    connect(&dataSocket_, &QTcpSocket::readyRead, this, &TncClient::onDataReadyRead);

    connect(&controlSocket_, &QTcpSocket::connected, this, [this]() {
        emit statusMessage(QStringLiteral("TNC control connected"));
        emitSocketState();
    });
    connect(&dataSocket_, &QTcpSocket::connected, this, [this]() {
        emit statusMessage(QStringLiteral("TNC data connected"));
        emitSocketState();
    });
    connect(&controlSocket_, &QTcpSocket::disconnected, this, [this]() {
        emit statusMessage(QStringLiteral("TNC control disconnected"));
        emitSocketState();
    });
    connect(&dataSocket_, &QTcpSocket::disconnected, this, [this]() {
        emit statusMessage(QStringLiteral("TNC data disconnected"));
        emitSocketState();
    });
    connect(&controlSocket_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (controlSocket_.error() == QAbstractSocket::ConnectionRefusedError)
            emit statusMessage(QStringLiteral("TNC control port is not ready yet"));
        else
            emit statusMessage(QStringLiteral("Control socket error: %1").arg(controlSocket_.errorString()));
        emitSocketState();
    });
    connect(&dataSocket_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (dataSocket_.error() == QAbstractSocket::ConnectionRefusedError)
            emit statusMessage(QStringLiteral("TNC data port is not ready yet"));
        else
            emit statusMessage(QStringLiteral("Data socket error: %1").arg(dataSocket_.errorString()));
        emitSocketState();
    });
}

bool TncClient::isControlConnected() const
{
    return controlSocket_.state() == QAbstractSocket::ConnectedState;
}

bool TncClient::isDataConnected() const
{
    return dataSocket_.state() == QAbstractSocket::ConnectedState;
}

void TncClient::connectToModem(const QString &host, quint16 basePort)
{
    disconnectFromModem();
    controlBuffer_.clear();
    controlSocket_.connectToHost(host, basePort);
    dataSocket_.connectToHost(host, static_cast<quint16>(basePort + 1));
    emit statusMessage(QStringLiteral("Connecting to Mercury TNC at %1:%2/%3")
                           .arg(host)
                           .arg(basePort)
                           .arg(basePort + 1));
}

void TncClient::disconnectFromModem()
{
    pendingCommands_.clear();
    controlSocket_.abort();
    dataSocket_.abort();
    emitSocketState();
}

void TncClient::initializeStation(const QString &callsign, int bandwidthHz)
{
    const QString normalizedCallsign = ChatProtocol::normalizeCallsign(callsign);
    if (normalizedCallsign.isEmpty())
    {
        emit statusMessage(QStringLiteral("Set a valid callsign before initializing the TNC"));
        return;
    }

    sendCommand(QStringLiteral("MYCALL %1").arg(normalizedCallsign));
    sendCommand(QStringLiteral("BW%1").arg(bandwidthHz));
    sendCommand(QStringLiteral("CHAT ON"));
    sendCommand(QStringLiteral("LISTEN ON"));
}

void TncClient::sendCommand(const QString &command)
{
    if (!isControlConnected())
    {
        emit statusMessage(QStringLiteral("Cannot send command while TNC control socket is disconnected"));
        return;
    }

    QByteArray bytes = command.toUtf8();
    bytes.append('\r');
    pendingCommands_.enqueue(command);
    controlSocket_.write(bytes);
    controlSocket_.flush();
}

void TncClient::sendCqFrame(const QString &callsign, int bandwidthHz)
{
    const QString normalizedCallsign = ChatProtocol::normalizeCallsign(callsign);
    if (normalizedCallsign.isEmpty())
    {
        emit statusMessage(QStringLiteral("Set a valid callsign before sending a beacon"));
        return;
    }

    sendCommand(QStringLiteral("CQFRAME %1 %2").arg(normalizedCallsign).arg(bandwidthHz));
}

void TncClient::connectPeer(const QString &myCallsign, const QString &theirCallsign)
{
    const QString my = ChatProtocol::normalizeCallsign(myCallsign);
    const QString their = ChatProtocol::normalizeCallsign(theirCallsign);
    if (my.isEmpty() || their.isEmpty())
    {
        emit statusMessage(QStringLiteral("Both local and remote callsigns are required"));
        return;
    }

    sendCommand(QStringLiteral("CONNECT %1 %2").arg(my, their));
}

void TncClient::disconnectLink()
{
    sendCommand(QStringLiteral("DISCONNECT"));
}

void TncClient::abortLink()
{
    sendCommand(QStringLiteral("ABORT"));
}

void TncClient::queryBuffer()
{
    if (!isControlConnected())
        return;

    controlSocket_.write("BUFFER\r");
    controlSocket_.flush();
}

void TncClient::sendPayload(const QByteArray &payload)
{
    if (!isDataConnected())
    {
        emit statusMessage(QStringLiteral("Cannot send chat data while TNC data socket is disconnected"));
        return;
    }

    dataSocket_.write(payload);
    dataSocket_.flush();
}

void TncClient::onControlReadyRead()
{
    controlBuffer_.append(controlSocket_.readAll());

    while (true)
    {
        const qsizetype lineEnd = controlBuffer_.indexOf('\r');
        if (lineEnd < 0)
            break;

        const QByteArray rawLine = controlBuffer_.left(lineEnd);
        controlBuffer_.remove(0, lineEnd + 1);
        const QString line = QString::fromUtf8(rawLine).trimmed();
        if (!line.isEmpty())
            parseControlLine(line);
    }
}

void TncClient::onDataReadyRead()
{
    emit dataReceived(dataSocket_.readAll());
}

void TncClient::emitSocketState()
{
    emit connectionStateChanged(isControlConnected(), isDataConnected());
}

void TncClient::parseControlLine(const QString &line)
{
    emit controlLineReceived(line);

    if (line == QLatin1String("OK") || line == QLatin1String("WRONG"))
    {
        const QString command = pendingCommands_.isEmpty() ? QString() : pendingCommands_.dequeue();
        emit commandResponse(line);
        emit commandCompleted(command, line);
        return;
    }

    const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return;

    const QString verb = parts.first().toUpper();
    if (verb == QLatin1String("CQFRAME") && parts.size() >= 3)
    {
        emit cqFrameReceived(ChatProtocol::normalizeCallsign(parts.at(1)), parts.at(2).toInt());
    }
    else if (verb == QLatin1String("CONNECTED") && parts.size() >= 4)
    {
        emit linkConnected(ChatProtocol::normalizeCallsign(parts.at(1)),
                           ChatProtocol::normalizeCallsign(parts.at(2)),
                           parts.at(3).toInt());
    }
    else if (verb == QLatin1String("DISCONNECTED"))
    {
        emit linkDisconnected();
    }
    else if (verb == QLatin1String("PENDING"))
    {
        emit pendingChanged(true);
    }
    else if (verb == QLatin1String("CANCELPENDING"))
    {
        emit pendingChanged(false);
    }
    else if (verb == QLatin1String("PTT") && parts.size() >= 2)
    {
        emit pttChanged(parts.at(1).compare(QStringLiteral("ON"), Qt::CaseInsensitive) == 0);
    }
    else if (verb == QLatin1String("BUFFER") && parts.size() >= 2)
    {
        emit bufferUpdated(parts.at(1).toInt());
    }
    else if (verb == QLatin1String("SN") && parts.size() >= 2)
    {
        emit snrUpdated(parts.at(1).toDouble());
    }
    else if (verb == QLatin1String("BITRATE"))
    {
        static const QRegularExpression re(QStringLiteral("^BITRATE\\s+\\((\\d+)\\)\\s+(\\d+)\\s+BPS$"));
        const QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch())
            emit bitrateUpdated(match.captured(1).toInt(), match.captured(2).toInt());
    }
}
