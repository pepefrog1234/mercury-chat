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
        Typing,
        Raw
    };

    Kind kind = Kind::Text;
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
    static QByteArray encodeTextMessage(const QString &from, const QString &text);
    static QByteArray encodeTypingNotification(const QString &from);
    static QList<ChatMessage> appendAndDecode(QByteArray &buffer, const QByteArray &chunk);
    static ChatPartialMessage previewIncompleteMessage(const QByteArray &buffer);
    static QString normalizeCallsign(const QString &callsign);

private:
    static ChatMessage decodeLine(const QByteArray &line);
};
