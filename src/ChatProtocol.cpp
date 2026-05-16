#include "ChatProtocol.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace
{
constexpr qsizetype kMaxBufferedBytes = 1024 * 1024;
}

QByteArray ChatProtocol::encodeTextMessage(const QString &from, const QString &text)
{
    QJsonObject object;
    object.insert(QStringLiteral("v"), 1);
    object.insert(QStringLiteral("type"), QStringLiteral("msg"));
    object.insert(QStringLiteral("from"), normalizeCallsign(from));
    object.insert(QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("text"), text);

    QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

QList<ChatMessage> ChatProtocol::appendAndDecode(QByteArray &buffer, const QByteArray &chunk)
{
    QList<ChatMessage> messages;
    buffer.append(chunk);

    while (true)
    {
        const qsizetype newlineIndex = buffer.indexOf('\n');
        if (newlineIndex < 0)
            break;

        QByteArray line = buffer.left(newlineIndex);
        buffer.remove(0, newlineIndex + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.trimmed().isEmpty())
            continue;

        messages.append(decodeLine(line));
    }

    if (buffer.size() > kMaxBufferedBytes)
    {
        ChatMessage raw;
        raw.kind = ChatMessage::Kind::Raw;
        raw.text = QString::fromUtf8(buffer);
        raw.timestampUtc = QDateTime::currentDateTimeUtc();
        messages.append(raw);
        buffer.clear();
    }

    return messages;
}

QString ChatProtocol::normalizeCallsign(const QString &callsign)
{
    const QString trimmed = callsign.trimmed().toUpper();
    QString normalized;
    normalized.reserve(qMin(trimmed.size(), 15));

    for (const QChar ch : trimmed)
    {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('/'))
            normalized.append(ch);
        if (normalized.size() >= 15)
            break;
    }

    return normalized;
}

ChatMessage ChatProtocol::decodeLine(const QByteArray &line)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line, &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        ChatMessage raw;
        raw.kind = ChatMessage::Kind::Raw;
        raw.text = QString::fromUtf8(line);
        raw.timestampUtc = QDateTime::currentDateTimeUtc();
        return raw;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("type")).toString() != QLatin1String("msg"))
    {
        ChatMessage raw;
        raw.kind = ChatMessage::Kind::Raw;
        raw.text = QString::fromUtf8(line);
        raw.timestampUtc = QDateTime::currentDateTimeUtc();
        return raw;
    }

    ChatMessage message;
    message.kind = ChatMessage::Kind::Text;
    message.from = normalizeCallsign(object.value(QStringLiteral("from")).toString());
    message.text = object.value(QStringLiteral("text")).toString();
    message.timestampUtc = QDateTime::fromString(object.value(QStringLiteral("time")).toString(), Qt::ISODateWithMs);
    if (!message.timestampUtc.isValid())
        message.timestampUtc = QDateTime::currentDateTimeUtc();
    return message;
}

