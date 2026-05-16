#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

class ModemProcess : public QObject
{
    Q_OBJECT

public:
    explicit ModemProcess(QObject *parent = nullptr);
    ~ModemProcess() override;

    bool isRunning() const;
    QString defaultExecutablePath() const;

public slots:
    void start(const QString &executablePath, const QStringList &arguments, const QString &workingDirectory = QString());
    void stop();

signals:
    void runningChanged(bool running);
    void statusMessage(const QString &message);
    void outputLine(const QString &line);

private slots:
    void readProcessOutput();

private:
    QStringList executableCandidates() const;
    QString normalizeExecutablePath(const QString &path) const;

    QProcess process_;
    QByteArray outputBuffer_;
};

