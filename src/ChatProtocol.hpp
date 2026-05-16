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
        Raw
    };

    Kind kind = Kind::Text;
    QString from;
    QString text;
    QDateTime timestampUtc;
};

class ChatProtocol
{
public:
    static QByteArray encodeTextMessage(const QString &from, const QString &text);
    static QList<ChatMessage> appendAndDecode(QByteArray &buffer, const QByteArray &chunk);
    static QString normalizeCallsign(const QString &callsign);

private:
    static ChatMessage decodeLine(const QByteArray &line);
};

