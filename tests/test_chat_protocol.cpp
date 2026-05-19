#include "ChatProtocol.hpp"
#include "SqlLogStore.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition)
        qCritical() << message;
    return condition;
}

bool expectRowCount(const QString &databasePath, const QString &tableName, int expectedCount)
{
    const QString connectionName = QStringLiteral("sql-log-test-reader");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databasePath);
        if (!expect(db.open(), "test database should reopen for verification"))
            return false;

        QSqlQuery query(db);
        if (!expect(query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(tableName)),
                    "row count query should execute"))
            return false;
        if (!expect(query.next(), "row count query should return a row"))
            return false;
        if (!expect(query.value(0).toInt() == expectedCount, "row count should match expected value"))
            return false;
    }
    QSqlDatabase::removeDatabase(connectionName);
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir sqlDir;
    if (!expect(sqlDir.isValid(), "temporary SQL directory should be valid"))
        return 1;
    const QString sqlPath = sqlDir.filePath(QStringLiteral("mercury-chat-test.sqlite3"));
    SqlLogStore store;
    QString sqlError;
    if (!expect(store.open(sqlPath, &sqlError), "SQL log store should open"))
    {
        qCritical() << sqlError;
        return 1;
    }
    if (!expect(store.recordBeacon(QDateTime::currentDateTimeUtc(),
                                   QStringLiteral("A"),
                                   QStringLiteral("TESTA"),
                                   QStringLiteral("TESTB"),
                                   500,
                                   true,
                                   12.5,
                                   &sqlError),
                "beacon log row should insert"))
    {
        qCritical() << sqlError;
        return 1;
    }
    const QDateTime firstChatAt = QDateTime::currentDateTimeUtc().addSecs(-20);
    const QDateTime secondChatAt = QDateTime::currentDateTimeUtc().addSecs(-10);
    const QDateTime otherChatAt = QDateTime::currentDateTimeUtc();
    if (!expect(store.recordChatMessage(firstChatAt,
                                        QStringLiteral("A"),
                                        QStringLiteral("out"),
                                        QStringLiteral("TESTA"),
                                        QStringLiteral("TESTB"),
                                        QStringLiteral("TESTA"),
                                        QStringLiteral("TESTB"),
                                        500,
                                        QStringLiteral("hello"),
                                        &sqlError),
                "chat log row should insert"))
    {
        qCritical() << sqlError;
        return 1;
    }
    if (!expect(store.recordChatMessage(secondChatAt,
                                        QStringLiteral("A"),
                                        QStringLiteral("in"),
                                        QStringLiteral("TESTA"),
                                        QStringLiteral("TESTB"),
                                        QStringLiteral("TESTA"),
                                        QStringLiteral("TESTB"),
                                        500,
                                        QStringLiteral("reply"),
                                        &sqlError),
                "second chat log row should insert"))
    {
        qCritical() << sqlError;
        return 1;
    }
    if (!expect(store.recordChatMessage(otherChatAt,
                                        QStringLiteral("A"),
                                        QStringLiteral("out"),
                                        QStringLiteral("TESTA"),
                                        QStringLiteral("TESTC"),
                                        QStringLiteral("TESTA"),
                                        QStringLiteral("TESTC"),
                                        500,
                                        QStringLiteral("other"),
                                        &sqlError),
                "other peer chat log row should insert"))
    {
        qCritical() << sqlError;
        return 1;
    }
    const QList<SqlLogStore::ChatLogEntry> history = store.loadChatHistory(QStringLiteral("TESTB"), 10, &sqlError);
    if (!expect(history.size() == 2, "chat history should load only requested peer"))
        return 1;
    if (!expect(history.at(0).body == QStringLiteral("hello"), "chat history should be chronological"))
        return 1;
    if (!expect(history.at(1).body == QStringLiteral("reply"), "chat history should include newer message second"))
        return 1;
    if (!expect(history.at(1).direction == QStringLiteral("in"), "chat history should preserve direction"))
        return 1;
    const QList<SqlLogStore::ChatLogEntry> limitedHistory = store.loadChatHistory(QStringLiteral("TESTB"), 1, &sqlError);
    if (!expect(limitedHistory.size() == 1, "limited chat history should return one row"))
        return 1;
    if (!expect(limitedHistory.first().body == QStringLiteral("reply"), "limited chat history should keep newest row"))
        return 1;
    store.close();
    if (!expectRowCount(sqlPath, QStringLiteral("beacon_events"), 1))
        return 1;
    if (!expectRowCount(sqlPath, QStringLiteral("chat_messages"), 3))
        return 1;

    QByteArray buffer;
    const QString sampleText = QStringLiteral("你好，世界\nMercury");
    const QByteArray encoded = ChatProtocol::encodeTextMessage(QStringLiteral("bv1-2"), sampleText);
    if (!expect(encoded.startsWith("MCHAT1 "), "encoded messages should declare payload size"))
        return 1;
    const QByteArray expectedCompactText = QByteArray("MCHAT1 ") +
                                           QByteArray::number(sampleText.toUtf8().size() + 1) +
                                           QByteArray("\nM") +
                                           sampleText.toUtf8();
    if (!expect(encoded == expectedCompactText, "text messages should use compact M payload without JSON overhead"))
        return 1;

    QList<ChatMessage> messages = ChatProtocol::appendAndDecode(buffer, encoded.left(8));
    if (!expect(messages.isEmpty(), "partial frame should not decode"))
        return 1;

    messages = ChatProtocol::appendAndDecode(buffer, encoded.mid(8));
    if (!expect(messages.size() == 1, "complete frame should decode one message"))
        return 1;
    if (!expect(messages.first().from.isEmpty(), "compact text should omit callsign"))
        return 1;
    if (!expect(messages.first().text == sampleText, "UTF-8 CJK text should round-trip"))
        return 1;
    if (!expect(buffer.isEmpty(), "buffer should be empty after complete decode"))
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
    const QByteArray typing = ChatProtocol::encodeTypingNotification(QStringLiteral("testb"));
    if (!expect(typing == QByteArray("MCHAT1 1\nT"), "typing notification should use compact one-byte payload"))
        return 1;
    messages = ChatProtocol::appendAndDecode(buffer, typing);
    if (!expect(messages.size() == 1, "typing notification should decode one message"))
        return 1;
    if (!expect(messages.first().kind == ChatMessage::Kind::Typing, "typing notification should use Typing kind"))
        return 1;
    if (!expect(messages.first().from.isEmpty(), "compact typing notification should omit callsign"))
        return 1;
    if (!expect(messages.first().text.isEmpty(), "typing notification should not carry chat text"))
        return 1;
    if (!expect(buffer.isEmpty(), "buffer should be empty after typing notification decode"))
        return 1;

    messages = ChatProtocol::appendAndDecode(buffer, QByteArray("T\n"));
    if (!expect(messages.size() == 1, "legacy raw T line should decode one message"))
        return 1;
    if (!expect(messages.first().kind == ChatMessage::Kind::Raw, "legacy raw T should not be treated as typing"))
        return 1;

    buffer.clear();
    const QByteArray partialEncoded =
        ChatProtocol::encodeTextMessage(QStringLiteral("TESTA"), QStringLiteral("眾神端坐高天原"));
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
    if (!expect(partial.from.isEmpty(), "compact partial preview should omit sender"))
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
