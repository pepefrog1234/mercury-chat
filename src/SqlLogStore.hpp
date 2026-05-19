#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>

class SqlLogStore
{
public:
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

private:
    bool initializeSchema(QString *errorMessage);
    bool execStatement(const QString &sql, QString *errorMessage);
    static QString timestampText(const QDateTime &dateTimeUtc);

    QSqlDatabase database_;
    QString connectionName_;
    QString databasePath_;
};
