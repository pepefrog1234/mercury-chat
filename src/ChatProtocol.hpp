#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

struct ChatMessage
{
    enum class Kind
    {
        Text,
        Ack,
        Raw
    };

    Kind kind = Kind::Text;
    QString id;
    QString ackId;
    QString from;
    QString text;
    QDateTime timestampUtc;
};

struct ChatPartialMessage
{
    bool active = false;
    QString from;
    QString text;
    qsizetype bytesBuffered = 0;
    qsizetype totalBytes = 0;
    bool totalBytesKnown = false;
};

class ChatProtocol
{
public:
    static QString createMessageId();
    static QByteArray encodeTextMessage(const QString &from, const QString &text, const QString &messageId = {});
    static QByteArray encodeAckMessage(const QString &from, const QString &messageId);
    static QList<ChatMessage> appendAndDecode(QByteArray &buffer, const QByteArray &chunk);
    static ChatPartialMessage previewIncompleteMessage(const QByteArray &buffer);
    static QString normalizeCallsign(const QString &callsign);

private:
    static ChatMessage decodeLine(const QByteArray &line);
};
