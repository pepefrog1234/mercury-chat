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
    const QByteArray encoded = ChatProtocol::encodeTextMessage(QStringLiteral("bv1-2"), QStringLiteral("你好，世界\nMercury"));

    QList<ChatMessage> messages = ChatProtocol::appendAndDecode(buffer, encoded.left(8));
    if (!expect(messages.isEmpty(), "partial frame should not decode"))
        return 1;

    messages = ChatProtocol::appendAndDecode(buffer, encoded.mid(8));
    if (!expect(messages.size() == 1, "complete frame should decode one message"))
        return 1;
    if (!expect(messages.first().from == QStringLiteral("BV1-2"), "callsign should normalize to uppercase"))
        return 1;
    if (!expect(messages.first().text == QStringLiteral("你好，世界\nMercury"), "UTF-8 CJK text should round-trip"))
        return 1;
    if (!expect(buffer.isEmpty(), "buffer should be empty after complete decode"))
        return 1;

    messages = ChatProtocol::appendAndDecode(buffer, QByteArray("plain utf8 \xe4\xbd\xa0\xe5\xa5\xbd\n"));
    if (!expect(messages.size() == 1, "raw line should decode one message"))
        return 1;
    if (!expect(messages.first().kind == ChatMessage::Kind::Raw, "invalid JSON line should be delivered as raw text"))
        return 1;

    return 0;
}

