#include "SqlLogStore.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

SqlLogStore::SqlLogStore()
    : connectionName_(QStringLiteral("mercury-chat-%1")
                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

SqlLogStore::~SqlLogStore()
{
    close();
}

bool SqlLogStore::open(const QString &databasePath, QString *errorMessage)
{
    close();

    const QFileInfo info(databasePath);
    if (!QDir().mkpath(info.absolutePath()))
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot create database directory: %1").arg(info.absolutePath());
        return false;
    }

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(info.absoluteFilePath());
    if (!database_.open())
    {
        if (errorMessage)
            *errorMessage = database_.lastError().text();
        close();
        return false;
    }

    databasePath_ = info.absoluteFilePath();
    if (!initializeSchema(errorMessage))
    {
        close();
        return false;
    }

    return true;
}

void SqlLogStore::close()
{
    const QString connectionName = connectionName_;
    if (database_.isValid())
    {
        database_.close();
        database_ = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    }
    databasePath_.clear();
}

bool SqlLogStore::isOpen() const
{
    return database_.isValid() && database_.isOpen();
}

QString SqlLogStore::databasePath() const
{
    return databasePath_;
}

bool SqlLogStore::recordBeacon(const QDateTime &heardAtUtc,
                               const QString &profile,
                               const QString &localCallsign,
                               const QString &remoteCallsign,
                               int bandwidthHz,
                               bool hasSnr,
                               double snrDb,
                               QString *errorMessage)
{
    if (!isOpen())
        return false;

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO beacon_events "
        "(heard_at, profile, local_call, remote_call, bandwidth_hz, snr_db, has_snr) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(timestampText(heardAtUtc));
    query.addBindValue(profile);
    query.addBindValue(localCallsign);
    query.addBindValue(remoteCallsign);
    query.addBindValue(bandwidthHz);
    query.addBindValue(hasSnr ? QVariant(snrDb) : QVariant());
    query.addBindValue(hasSnr ? 1 : 0);

    if (!query.exec())
    {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

bool SqlLogStore::recordChatMessage(const QDateTime &messageAtUtc,
                                    const QString &profile,
                                    const QString &direction,
                                    const QString &localCallsign,
                                    const QString &remoteCallsign,
                                    const QString &linkSource,
                                    const QString &linkDestination,
                                    int bandwidthHz,
                                    const QString &body,
                                    QString *errorMessage)
{
    if (!isOpen())
        return false;

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO chat_messages "
        "(message_at, profile, direction, local_call, remote_call, link_source, link_destination, bandwidth_hz, body) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(timestampText(messageAtUtc));
    query.addBindValue(profile);
    query.addBindValue(direction);
    query.addBindValue(localCallsign);
    query.addBindValue(remoteCallsign);
    query.addBindValue(linkSource);
    query.addBindValue(linkDestination);
    query.addBindValue(bandwidthHz > 0 ? QVariant(bandwidthHz) : QVariant());
    query.addBindValue(body);

    if (!query.exec())
    {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }

    return true;
}

bool SqlLogStore::initializeSchema(QString *errorMessage)
{
    if (!execStatement(QStringLiteral("PRAGMA foreign_keys = ON"), errorMessage))
        return false;
    if (!execStatement(QStringLiteral("PRAGMA journal_mode = WAL"), errorMessage))
        return false;

    if (!execStatement(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS beacon_events ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "heard_at TEXT NOT NULL,"
            "profile TEXT,"
            "local_call TEXT,"
            "remote_call TEXT NOT NULL,"
            "bandwidth_hz INTEGER NOT NULL,"
            "snr_db REAL,"
            "has_snr INTEGER NOT NULL DEFAULT 0 CHECK(has_snr IN (0, 1)),"
            "source TEXT NOT NULL DEFAULT 'tnc_cqframe',"
            "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
            ")"),
        errorMessage))
        return false;

    if (!execStatement(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_beacon_events_remote_time "
            "ON beacon_events(remote_call, heard_at)"),
        errorMessage))
        return false;

    if (!execStatement(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS chat_messages ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "message_at TEXT NOT NULL,"
            "profile TEXT,"
            "direction TEXT NOT NULL CHECK(direction IN ('in', 'out')),"
            "local_call TEXT,"
            "remote_call TEXT,"
            "link_source TEXT,"
            "link_destination TEXT,"
            "bandwidth_hz INTEGER,"
            "body TEXT NOT NULL,"
            "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))"
            ")"),
        errorMessage))
        return false;

    if (!execStatement(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_chat_messages_remote_time "
            "ON chat_messages(remote_call, message_at)"),
        errorMessage))
        return false;

    if (!execStatement(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_chat_messages_time "
            "ON chat_messages(message_at)"),
        errorMessage))
        return false;

    return execStatement(QStringLiteral("PRAGMA user_version = 1"), errorMessage);
}

bool SqlLogStore::execStatement(const QString &sql, QString *errorMessage)
{
    QSqlQuery query(database_);
    if (!query.exec(sql))
    {
        if (errorMessage)
            *errorMessage = query.lastError().text();
        return false;
    }
    return true;
}

QString SqlLogStore::timestampText(const QDateTime &dateTimeUtc)
{
    QDateTime normalized = dateTimeUtc;
    if (!normalized.isValid())
        normalized = QDateTime::currentDateTimeUtc();
    return normalized.toUTC().toString(Qt::ISODateWithMs);
}
