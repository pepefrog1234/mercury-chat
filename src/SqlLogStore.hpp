#pragma once

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>

class SqlLogStore
{
public:
    struct ChatLogEntry
    {
        QDateTime messageAtUtc;
        QString direction;
        QString localCallsign;
        QString remoteCallsign;
        QString body;
    };

    SqlLogStore();
    ~SqlLogStore();

    bool open(const QString &databasePath, QString *errorMessage = nullptr);
    void close();
    bool isOpen() const;
    QString databasePath() const;

    bool recordBeacon(const QDateTime &heardAtUtc,
                      const QString &profile,
                      const QString &localCallsign,
                      const QString &remoteCallsign,
                      int bandwidthHz,
                      bool hasSnr,
                      double snrDb,
                      QString *errorMessage = nullptr);

    bool recordChatMessage(const QDateTime &messageAtUtc,
                           const QString &profile,
                           const QString &direction,
                           const QString &localCallsign,
                           const QString &remoteCallsign,
                           const QString &linkSource,
                           const QString &linkDestination,
                           int bandwidthHz,
                           const QString &body,
                           QString *errorMessage = nullptr);

    QList<ChatLogEntry> loadChatHistory(const QString &remoteCallsign,
                                        int limit,
                                        QString *errorMessage = nullptr) const;

private:
    bool initializeSchema(QString *errorMessage);
    bool execStatement(const QString &sql, QString *errorMessage);
    static QString timestampText(const QDateTime &dateTimeUtc);
    static QDateTime parseTimestampText(const QString &text);

    QSqlDatabase database_;
    QString connectionName_;
    QString databasePath_;
};
