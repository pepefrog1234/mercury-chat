#include "ChatProtocol.hpp"

#include <QCoreApplication>
#include <QDebug>

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition)
        qCritical() << message;
    return condition;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QByteArray buffer;
    const QString firstMessageId = ChatProtocol::createMessageId();
    if (!expect(!firstMessageId.isEmpty(), "generated message id should not be empty"))
        return 1;
    const QByteArray encoded = ChatProtocol::encodeTextMessage(QStringLiteral("bv1-2"), QStringLiteral("你好，世界\nMercury"), firstMessageId);
    if (!expect(encoded.startsWith("MCHAT1 "), "encoded messages should declare payload size before JSON"))
        return 1;

    QList<ChatMessage> messages = ChatProtocol::appendAndDecode(buffer, encoded.left(8));
    if (!expect(messages.isEmpty(), "partial frame should not decode"))
        return 1;

    messages = ChatProtocol::appendAndDecode(buffer, encoded.mid(8));
    if (!expect(messages.size() == 1, "complete frame should decode one message"))
        return 1;
    if (!expect(messages.first().from == QStringLiteral("BV1-2"), "callsign should normalize to uppercase"))
        return 1;
    if (!expect(messages.first().id == firstMessageId, "message id should round-trip"))
        return 1;
    if (!expect(messages.first().text == QStringLiteral("你好，世界\nMercury"), "UTF-8 CJK text should round-trip"))
        return 1;
    if (!expect(buffer.isEmpty(), "buffer should be empty after complete decode"))
        return 1;

    const QByteArray ackEncoded = ChatProtocol::encodeAckMessage(QStringLiteral("TESTB"), firstMessageId, 5);
    messages = ChatProtocol::appendAndDecode(buffer, ackEncoded);
    if (!expect(messages.size() == 1, "ack frame should decode one message"))
        return 1;
    if (!expect(messages.first().kind == ChatMessage::Kind::Ack, "ack frame should decode as ack"))
        return 1;
    if (!expect(messages.first().ackId == firstMessageId, "ack id should match the original message id"))
        return 1;
    if (!expect(messages.first().ackChars == 5, "ack should carry delivered character count"))
        return 1;
    if (!expect(messages.first().from == QStringLiteral("TESTB"), "ack sender should normalize"))
        return 1;

    const QString chunkMessageId = ChatProtocol::createMessageId();
    const QByteArray chunkEncoded =
        ChatProtocol::encodeTextChunk(QStringLiteral("TESTA"), chunkMessageId, 3, 9, QStringLiteral("def"), false);
    messages = ChatProtocol::appendAndDecode(buffer, chunkEncoded);
    if (!expect(messages.size() == 1, "chunk frame should decode one message"))
        return 1;
    if (!expect(messages.first().id == chunkMessageId, "chunk id should decode"))
        return 1;
    if (!expect(messages.first().offset == 3, "chunk offset should decode"))
        return 1;
    if (!expect(messages.first().totalChars == 9, "chunk total should decode"))
        return 1;
    if (!expect(!messages.first().finalChunk, "chunk final flag should decode"))
        return 1;
    if (!expect(messages.first().text == QStringLiteral("def"), "chunk text should decode"))
        return 1;

    const QByteArray legacyJsonLine = "{\"from\":\"TESTA\",\"text\":\"legacy\",\"time\":\"2026-01-01T00:00:00.000Z\",\"type\":\"msg\",\"v\":1}\n";
    messages = ChatProtocol::appendAndDecode(buffer, legacyJsonLine);
    if (!expect(messages.size() == 1, "legacy newline JSON should still decode"))
        return 1;
    if (!expect(messages.first().text == QStringLiteral("legacy"), "legacy JSON text should decode"))
        return 1;

    messages = ChatProtocol::appendAndDecode(buffer, QByteArray("plain utf8 \xe4\xbd\xa0\xe5\xa5\xbd\n"));
    if (!expect(messages.size() == 1, "raw line should decode one message"))
        return 1;
    if (!expect(messages.first().kind == ChatMessage::Kind::Raw, "invalid JSON line should be delivered as raw text"))
        return 1;

    buffer.clear();
    const QString partialMessageId = ChatProtocol::createMessageId();
    const QByteArray partialEncoded =
        ChatProtocol::encodeTextMessage(QStringLiteral("TESTA"), QStringLiteral("眾神端坐高天原"), partialMessageId);
    const qsizetype headerEnd = partialEncoded.indexOf('\n') + 1;
    buffer = partialEncoded.left(headerEnd);
    ChatPartialMessage partial = ChatProtocol::previewIncompleteMessage(buffer);
    if (!expect(partial.totalBytesKnown, "frame header should expose total byte count before text arrives"))
        return 1;
    if (!expect(partial.bytesBuffered == 0, "header-only preview should report zero payload bytes"))
        return 1;
    if (!expect(partial.totalBytes > 0, "frame header should declare a positive payload size"))
        return 1;

    const QByteArray visiblePrefix = QStringLiteral("眾神").toUtf8();
    const qsizetype visibleEnd = partialEncoded.indexOf(visiblePrefix) + visiblePrefix.size();
    buffer = partialEncoded.left(visibleEnd);
    partial = ChatProtocol::previewIncompleteMessage(buffer);
    if (!expect(partial.active, "partial JSON text should produce a preview"))
        return 1;
    if (!expect(partial.totalBytesKnown, "length-prefixed partial preview should know total byte count"))
        return 1;
    if (!expect(partial.bytesBuffered < partial.totalBytes, "partial preview should report received bytes before completion"))
        return 1;
    if (!expect(partial.from == QStringLiteral("TESTA"), "partial preview should include sender once decoded"))
        return 1;
    if (!expect(partial.id == partialMessageId, "partial preview should expose message id for progress ack"))
        return 1;
    if (!expect(partial.text == QStringLiteral("眾神"), "partial preview should expose decoded CJK prefix"))
        return 1;

    const QByteArray nextCharacter = QStringLiteral("端").toUtf8();
    buffer = partialEncoded.left(visibleEnd + 1);
    partial = ChatProtocol::previewIncompleteMessage(buffer);
    if (!expect(partial.text == QStringLiteral("眾神"), "partial preview should not show broken UTF-8 characters"))
        return 1;

    buffer = partialEncoded.left(visibleEnd + nextCharacter.size());
    partial = ChatProtocol::previewIncompleteMessage(buffer);
    if (!expect(partial.text == QStringLiteral("眾神端"), "partial preview should advance after a full UTF-8 character"))
        return 1;

    return 0;
}
