#include "ChatProtocol.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cstring>

namespace
{
constexpr qsizetype kMaxBufferedBytes = 1024 * 1024;
constexpr auto kFrameMagic = "MCHAT1 ";

struct LengthHeader
{
    bool present = false;
    bool complete = false;
    bool valid = false;
    qsizetype headerBytes = 0;
    qsizetype payloadBytes = 0;
};

LengthHeader parseLengthHeader(const QByteArray &buffer)
{
    LengthHeader header;
    if (!buffer.startsWith(kFrameMagic))
        return header;

    header.present = true;
    const qsizetype newlineIndex = buffer.indexOf('\n');
    if (newlineIndex < 0)
        return header;

    header.complete = true;
    header.headerBytes = newlineIndex + 1;

    QByteArray sizeBytes = buffer.mid(static_cast<qsizetype>(strlen(kFrameMagic)),
                                      newlineIndex - static_cast<qsizetype>(strlen(kFrameMagic)));
    if (sizeBytes.endsWith('\r'))
        sizeBytes.chop(1);
    sizeBytes = sizeBytes.trimmed();

    bool ok = false;
    const qlonglong payloadBytes = sizeBytes.toLongLong(&ok);
    if (!ok || payloadBytes < 0 || payloadBytes > kMaxBufferedBytes)
        return header;

    header.valid = true;
    header.payloadBytes = static_cast<qsizetype>(payloadBytes);
    return header;
}

ChatMessage rawMessageFromLine(const QByteArray &line)
{
    ChatMessage raw;
    raw.kind = ChatMessage::Kind::Raw;
    raw.text = QString::fromUtf8(line);
    raw.timestampUtc = QDateTime::currentDateTimeUtc();
    return raw;
}

qsizetype completeUtf8PrefixLength(const QByteArray &bytes)
{
    qsizetype i = 0;
    qsizetype lastComplete = 0;

    while (i < bytes.size())
    {
        const uchar first = static_cast<uchar>(bytes.at(i));
        qsizetype sequenceLength = 0;

        if (first < 0x80)
            sequenceLength = 1;
        else if ((first & 0xE0) == 0xC0)
            sequenceLength = 2;
        else if ((first & 0xF0) == 0xE0)
            sequenceLength = 3;
        else if ((first & 0xF8) == 0xF0)
            sequenceLength = 4;
        else
            break;

        if (i + sequenceLength > bytes.size())
            break;

        bool valid = true;
        for (qsizetype j = 1; j < sequenceLength; ++j)
        {
            const uchar continuation = static_cast<uchar>(bytes.at(i + j));
            if ((continuation & 0xC0) != 0x80)
            {
                valid = false;
                break;
            }
        }

        if (!valid)
            break;

        i += sequenceLength;
        lastComplete = i;
    }

    return lastComplete;
}

bool hexValue(char ch, uint *value)
{
    if (!value)
        return false;

    if (ch >= '0' && ch <= '9')
        *value = static_cast<uint>(ch - '0');
    else if (ch >= 'a' && ch <= 'f')
        *value = static_cast<uint>(ch - 'a' + 10);
    else if (ch >= 'A' && ch <= 'F')
        *value = static_cast<uint>(ch - 'A' + 10);
    else
        return false;

    return true;
}

void flushUtf8Run(QString &out, QByteArray &run)
{
    if (run.isEmpty())
        return;

    const qsizetype prefixLength = completeUtf8PrefixLength(run);
    if (prefixLength > 0)
        out.append(QString::fromUtf8(run.constData(), prefixLength));
    run.clear();
}

QString decodeJsonStringPrefix(const QByteArray &bytes, qsizetype start)
{
    QString out;
    QByteArray utf8Run;

    for (qsizetype i = start; i < bytes.size(); ++i)
    {
        const char ch = bytes.at(i);

        if (ch == '"')
        {
            flushUtf8Run(out, utf8Run);
            return out;
        }

        if (ch != '\\')
        {
            utf8Run.append(ch);
            continue;
        }

        flushUtf8Run(out, utf8Run);
        if (++i >= bytes.size())
            return out;

        const char escaped = bytes.at(i);
        switch (escaped)
        {
        case '"': out.append(QLatin1Char('"')); break;
        case '\\': out.append(QLatin1Char('\\')); break;
        case '/': out.append(QLatin1Char('/')); break;
        case 'b': out.append(QLatin1Char('\b')); break;
        case 'f': out.append(QLatin1Char('\f')); break;
        case 'n': out.append(QLatin1Char('\n')); break;
        case 'r': out.append(QLatin1Char('\r')); break;
        case 't': out.append(QLatin1Char('\t')); break;
        case 'u':
        {
            if (i + 4 >= bytes.size())
                return out;

            uint codepoint = 0;
            for (int j = 0; j < 4; ++j)
            {
                uint nibble = 0;
                if (!hexValue(bytes.at(i + 1 + j), &nibble))
                    return out;
                codepoint = (codepoint << 4) | nibble;
            }
            i += 4;
            out.append(QChar(static_cast<ushort>(codepoint)));
            break;
        }
        default:
            out.append(QLatin1Char(escaped));
            break;
        }
    }

    flushUtf8Run(out, utf8Run);
    return out;
}

QString previewStringField(const QByteArray &bytes, const char *key)
{
    const QByteArray needle = QByteArray("\"") + key + "\":\"";
    const qsizetype fieldStart = bytes.indexOf(needle);
    if (fieldStart < 0)
        return {};

    return decodeJsonStringPrefix(bytes, fieldStart + needle.size());
}
}

QByteArray ChatProtocol::encodeTextMessage(const QString &from, const QString &text)
{
    QJsonObject object;
    object.insert(QStringLiteral("v"), 1);
    object.insert(QStringLiteral("type"), QStringLiteral("msg"));
    object.insert(QStringLiteral("from"), normalizeCallsign(from));
    object.insert(QStringLiteral("time"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("text"), text);

    QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QByteArray frame = QByteArray(kFrameMagic) + QByteArray::number(payload.size()) + '\n';
    frame.append(payload);
    frame.append('\n');
    return frame;
}

QList<ChatMessage> ChatProtocol::appendAndDecode(QByteArray &buffer, const QByteArray &chunk)
{
    QList<ChatMessage> messages;
    buffer.append(chunk);

    while (true)
    {
        const LengthHeader header = parseLengthHeader(buffer);
        if (header.present)
        {
            if (!header.complete)
                break;

            if (!header.valid)
            {
                QByteArray line = buffer.left(header.headerBytes - 1);
                if (line.endsWith('\r'))
                    line.chop(1);
                buffer.remove(0, header.headerBytes);
                if (!line.trimmed().isEmpty())
                    messages.append(rawMessageFromLine(line));
                continue;
            }

            if (buffer.size() < header.headerBytes + header.payloadBytes)
                break;

            QByteArray payload = buffer.mid(header.headerBytes, header.payloadBytes);
            buffer.remove(0, header.headerBytes + header.payloadBytes);
            if (buffer.startsWith("\r\n"))
                buffer.remove(0, 2);
            else if (buffer.startsWith('\n'))
                buffer.remove(0, 1);

            if (payload.trimmed().isEmpty())
                continue;

            messages.append(decodeLine(payload));
            continue;
        }

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
        messages.append(rawMessageFromLine(buffer));
        buffer.clear();
    }

    return messages;
}

ChatPartialMessage ChatProtocol::previewIncompleteMessage(const QByteArray &buffer)
{
    ChatPartialMessage preview;
    if (buffer.isEmpty())
        return preview;

    QByteArray visibleBuffer = buffer;
    const LengthHeader header = parseLengthHeader(buffer);
    if (header.present)
    {
        if (!header.complete || !header.valid)
            return preview;

        const qsizetype availablePayloadBytes = qMax<qsizetype>(0, buffer.size() - header.headerBytes);
        preview.bytesBuffered = qMin(availablePayloadBytes, header.payloadBytes);
        preview.totalBytes = header.payloadBytes;
        preview.totalBytesKnown = true;
        visibleBuffer = buffer.mid(header.headerBytes, preview.bytesBuffered);
    }
    else
    {
        preview.bytesBuffered = buffer.size();
    }

    preview.from = normalizeCallsign(previewStringField(visibleBuffer, "from"));
    preview.text = previewStringField(visibleBuffer, "text");
    preview.active = !preview.text.isEmpty();
    return preview;
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
        return rawMessageFromLine(line);
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("type")).toString() != QLatin1String("msg"))
    {
        return rawMessageFromLine(line);
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
