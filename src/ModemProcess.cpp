#include "ModemProcess.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace
{
QString executableName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("mercury.exe");
#else
    return QStringLiteral("mercury");
#endif
}
}

ModemProcess::ModemProcess(QObject *parent)
    : QObject(parent),
      process_(this)
{
    process_.setProcessChannelMode(QProcess::MergedChannels);

    connect(&process_, &QProcess::started, this, [this]() {
        emit runningChanged(true);
        emit statusMessage(QStringLiteral("Mercury modem started"));
    });

    connect(&process_, &QProcess::readyReadStandardOutput, this, &ModemProcess::readProcessOutput);

    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
        {
            emit statusMessage(QStringLiteral("Could not start Mercury modem: %1").arg(process_.errorString()));
            emit runningChanged(false);
        }
    });

    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                readProcessOutput();
                const QString status = exitStatus == QProcess::CrashExit
                                           ? QStringLiteral("Mercury modem crashed")
                                           : QStringLiteral("Mercury modem exited with code %1").arg(exitCode);
                emit statusMessage(status);
                emit runningChanged(false);
            });
}

ModemProcess::~ModemProcess()
{
    stop();
}

bool ModemProcess::isRunning() const
{
    return process_.state() != QProcess::NotRunning;
}

QString ModemProcess::defaultExecutablePath() const
{
    for (const QString &candidate : executableCandidates())
    {
        const QString normalized = normalizeExecutablePath(candidate);
        if (!normalized.isEmpty())
            return normalized;
    }

    return executableName();
}

void ModemProcess::start(const QString &executablePath, const QStringList &arguments, const QString &workingDirectory)
{
    if (isRunning())
    {
        emit statusMessage(QStringLiteral("Mercury modem is already running"));
        return;
    }

    const QString executable = normalizeExecutablePath(executablePath);
    if (executable.isEmpty())
    {
        emit statusMessage(QStringLiteral("Mercury executable was not found. Build Mercury or set MERCURY_MODEM_PATH."));
        emit runningChanged(false);
        return;
    }

    QFileInfo executableInfo(executable);
    if (!workingDirectory.trimmed().isEmpty())
        process_.setWorkingDirectory(workingDirectory);
    else if (executableInfo.exists())
        process_.setWorkingDirectory(executableInfo.absolutePath());

    outputBuffer_.clear();
    emit statusMessage(QStringLiteral("Starting Mercury modem: %1 %2").arg(executable, arguments.join(QLatin1Char(' '))));
    process_.start(executable, arguments);
}

void ModemProcess::stop()
{
    if (!isRunning())
        return;

    process_.terminate();
    if (!process_.waitForFinished(3000))
    {
        process_.kill();
        process_.waitForFinished(2000);
    }
}

void ModemProcess::readProcessOutput()
{
    outputBuffer_.append(process_.readAllStandardOutput());

    while (true)
    {
        qsizetype lineEnd = outputBuffer_.indexOf('\n');
        if (lineEnd < 0)
            break;

        QByteArray line = outputBuffer_.left(lineEnd);
        outputBuffer_.remove(0, lineEnd + 1);
        if (line.endsWith('\r'))
            line.chop(1);

        const QString text = QString::fromLocal8Bit(line).trimmed();
        if (!text.isEmpty())
            emit outputLine(text);
    }
}

QStringList ModemProcess::executableCandidates() const
{
    const QString envPath = qEnvironmentVariable("MERCURY_MODEM_PATH");
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QDir currentDir(QDir::currentPath());
    const QString name = executableName();

    QStringList candidates;
    if (!envPath.trimmed().isEmpty())
        candidates << envPath;

    candidates << appDir.filePath(name);
    candidates << appDir.filePath(QStringLiteral("../") + name);
    candidates << appDir.filePath(QStringLiteral("../../mercury/") + name);
    candidates << currentDir.filePath(QStringLiteral("mercury/") + name);
    candidates << currentDir.filePath(name);
    candidates << QStandardPaths::findExecutable(name);

    return candidates;
}

QString ModemProcess::normalizeExecutablePath(const QString &path) const
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return {};

    QFileInfo info(trimmed);
    if (info.isFile() && info.isExecutable())
        return info.absoluteFilePath();

    const QString found = QStandardPaths::findExecutable(trimmed);
    if (!found.isEmpty())
        return found;

    return {};
}

