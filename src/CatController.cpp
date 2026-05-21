#include "CatController.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <algorithm>

#if MERCURYCHAT_WITH_HAMLIB
#include <cstdio>
#endif

namespace
{
QList<CatRigModel> parseRigctlModelList(const QString &text)
{
    QList<CatRigModel> models;
    const QRegularExpression linePattern(QStringLiteral("^\\s*(\\d+)\\s+(.+)$"));

    for (const QString &line : text.split(QLatin1Char('\n')))
    {
        const QRegularExpressionMatch match = linePattern.match(line);
        if (!match.hasMatch())
            continue;

        bool ok = false;
        const int modelId = match.captured(1).toInt(&ok);
        QString name = match.captured(2).simplified();
        if (!ok || modelId <= 0 || name.isEmpty())
            continue;
        if (name.startsWith(QStringLiteral("Mfg "), Qt::CaseInsensitive))
            continue;

        models.append({modelId, name});
    }

    std::sort(models.begin(), models.end(), [](const CatRigModel &left, const CatRigModel &right) {
        return QString::localeAwareCompare(left.name, right.name) < 0;
    });
    return models;
}

QString firstValueLine(const QStringList &lines)
{
    for (const QString &line : lines)
    {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith(QStringLiteral("RPRT ")))
            return trimmed;
    }
    return {};
}

#if MERCURYCHAT_WITH_HAMLIB
QString rigDisplayName(const struct rig_caps *caps)
{
    const QString manufacturer = QString::fromLocal8Bit(caps->mfg_name ? caps->mfg_name : "").trimmed();
    const QString model = QString::fromLocal8Bit(caps->model_name ? caps->model_name : "").trimmed();
    if (manufacturer.isEmpty())
        return model;
    if (model.isEmpty() || model.compare(manufacturer, Qt::CaseInsensitive) == 0)
        return manufacturer;
    return QStringLiteral("%1 %2").arg(manufacturer, model);
}

int collectRigModel(const struct rig_caps *caps, rig_ptr_t data)
{
    if (!caps || !data)
        return 1;

    auto *models = static_cast<QList<CatRigModel> *>(data);
    const QString name = rigDisplayName(caps);
    if (!name.isEmpty())
        models->append({static_cast<int>(caps->rig_model), name});
    return 1;
}

enum serial_control_state_e toHamlibSerialState(CatSerialLineState state)
{
    return static_cast<enum serial_control_state_e>(static_cast<int>(state));
}
#endif
}

CatController::CatController(QObject *parent)
    : QObject(parent)
{
}

CatController::~CatController()
{
    disconnectRig();
}

bool CatController::isConnected() const
{
    return connected_;
}

QList<CatRigModel> CatController::availableRigModels()
{
    QList<CatRigModel> models;

#if MERCURYCHAT_WITH_HAMLIB
    rig_set_debug(RIG_DEBUG_NONE);
    rig_load_all_backends();
    rig_list_foreach(collectRigModel, &models);
    std::sort(models.begin(), models.end(), [](const CatRigModel &left, const CatRigModel &right) {
        return QString::localeAwareCompare(left.name, right.name) < 0;
    });
    if (!models.isEmpty())
        return models;
#endif

    QProcess rigctl;
    rigctl.start(findHamlibTool(QStringLiteral("rigctl")), {QStringLiteral("-l")});
    if (!rigctl.waitForStarted(1500))
        return models;
    if (!rigctl.waitForFinished(5000))
    {
        rigctl.kill();
        rigctl.waitForFinished();
        return models;
    }

    return parseRigctlModelList(QString::fromLocal8Bit(rigctl.readAllStandardOutput()));
}

QString CatController::findHamlibTool(const QString &toolName)
{
    QString executable = toolName;
#ifdef Q_OS_WIN
    if (!executable.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
        executable += QStringLiteral(".exe");
#endif

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(executable),
        QDir(appDir).filePath(QStringLiteral("hamlib/bin/%1").arg(executable)),
        QDir(appDir).filePath(QStringLiteral("bin/%1").arg(executable)),
    };

    for (const QString &candidate : candidates)
    {
        if (QFileInfo::exists(candidate))
            return QDir::toNativeSeparators(candidate);
    }

    return executable;
}

bool CatController::parseHostPort(const QString &text, QString *host, quint16 *port)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("COM"), Qt::CaseInsensitive))
        return false;

    const int colon = trimmed.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0 || colon == trimmed.size() - 1)
        return false;

    bool ok = false;
    const int parsedPort = trimmed.mid(colon + 1).toInt(&ok);
    if (!ok || parsedPort <= 0 || parsedPort > 65535)
        return false;

    const QString parsedHost = trimmed.left(colon).trimmed();
    if (parsedHost.isEmpty())
        return false;

    if (host)
        *host = parsedHost;
    if (port)
        *port = static_cast<quint16>(parsedPort);
    return true;
}

QString CatController::pttTypeName(CatPttMethod pttMethod)
{
    switch (pttMethod)
    {
    case CatPttMethod::SerialRts:
        return QStringLiteral("RTS");
    case CatPttMethod::SerialDtr:
        return QStringLiteral("DTR");
    case CatPttMethod::Cat:
        return QStringLiteral("RIG");
    }
    return QStringLiteral("RIG");
}

QString CatController::serialLineStateName(CatSerialLineState state)
{
    switch (state)
    {
    case CatSerialLineState::On:
        return QStringLiteral("ON");
    case CatSerialLineState::Off:
        return QStringLiteral("OFF");
    case CatSerialLineState::Unset:
        return QStringLiteral("Unset");
    }
    return QStringLiteral("Unset");
}

bool CatController::connectToRigctld(const QString &host, quint16 port, int timeoutMs) const
{
    QTcpSocket socket;
    socket.connectToHost(host, port);
    return socket.waitForConnected(timeoutMs);
}

QStringList CatController::sendRigctldCommand(const QString &command, bool *ok, int timeoutMs) const
{
    if (ok)
        *ok = false;
    if (rigctldHost_.isEmpty() || rigctldPort_ == 0)
        return {};

    QTcpSocket socket;
    socket.connectToHost(rigctldHost_, rigctldPort_);
    if (!socket.waitForConnected(timeoutMs))
        return {};

    socket.write(command.toLocal8Bit());
    socket.write("\n", 1);
    if (!socket.waitForBytesWritten(timeoutMs))
        return {};

    QByteArray response;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs)
    {
        const int remaining = qMax(50, timeoutMs - static_cast<int>(timer.elapsed()));
        if (socket.waitForReadyRead(qMin(remaining, 250)))
        {
            response += socket.readAll();
            const QString responseText = QString::fromLocal8Bit(response);
            if (responseText.contains(QStringLiteral("\nRPRT ")) || responseText.startsWith(QStringLiteral("RPRT ")))
                break;
            continue;
        }

        if (!response.isEmpty())
            break;
    }

    QStringList lines;
    for (const QString &line : QString::fromLocal8Bit(response).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).split(QLatin1Char('\n')))
    {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            lines.append(trimmed);
    }

    bool success = !lines.isEmpty();
    for (const QString &line : lines)
    {
        if (!line.startsWith(QStringLiteral("RPRT ")))
            continue;
        bool reportOk = false;
        const int report = line.mid(5).trimmed().toInt(&reportOk);
        if (reportOk && report < 0)
            success = false;
    }

    if (ok)
        *ok = success;
    return lines;
}

bool CatController::startBundledRigctld(int modelId,
                                        const QString &devicePath,
                                        int serialSpeed,
                                        CatSerialLineState rtsState,
                                        CatSerialLineState dtrState,
                                        CatPttMethod pttMethod)
{
    const QString program = findHamlibTool(QStringLiteral("rigctld"));
    if (!QFileInfo::exists(program) && program.contains(QDir::separator()))
    {
        emit statusMessage(QStringLiteral("bundled rigctld was not found"));
        return false;
    }

    QTcpServer portProbe;
    if (!portProbe.listen(QHostAddress::LocalHost, 0))
    {
        emit statusMessage(QStringLiteral("could not allocate local rigctld port"));
        return false;
    }
    const quint16 port = portProbe.serverPort();
    portProbe.close();

    QStringList arguments;
    arguments << QStringLiteral("-m") << QString::number(modelId);
    arguments << QStringLiteral("-T") << QStringLiteral("127.0.0.1");
    arguments << QStringLiteral("-t") << QString::number(port);
    arguments << QStringLiteral("-P") << pttTypeName(pttMethod);

    const QString trimmedDevice = devicePath.trimmed();
    if (!trimmedDevice.isEmpty())
    {
        arguments << QStringLiteral("-r") << trimmedDevice;
        if (pttMethod != CatPttMethod::Cat)
            arguments << QStringLiteral("-p") << trimmedDevice;
    }
    if (serialSpeed > 0)
        arguments << QStringLiteral("-s") << QString::number(serialSpeed);

    QStringList conf;
    if (rtsState != CatSerialLineState::Unset)
        conf << QStringLiteral("rts_state=%1").arg(serialLineStateName(rtsState));
    if (dtrState != CatSerialLineState::Unset)
        conf << QStringLiteral("dtr_state=%1").arg(serialLineStateName(dtrState));
    if (!conf.isEmpty())
        arguments << QStringLiteral("-C") << conf.join(QLatin1Char(','));

    rigctldProcess_.setProgram(program);
    rigctldProcess_.setArguments(arguments);
    rigctldProcess_.setProcessChannelMode(QProcess::MergedChannels);
    rigctldProcess_.start();
    if (!rigctldProcess_.waitForStarted(3000))
    {
        emit statusMessage(QStringLiteral("rigctld failed to start: %1").arg(rigctldProcess_.errorString()));
        return false;
    }

    rigctldHost_ = QStringLiteral("127.0.0.1");
    rigctldPort_ = port;
    rigctldManaged_ = true;

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000)
    {
        if (connectToRigctld(rigctldHost_, rigctldPort_, 250))
            return true;
        if (rigctldProcess_.state() == QProcess::NotRunning)
            break;
    }

    const QString output = QString::fromLocal8Bit(rigctldProcess_.readAll()).trimmed();
    emit statusMessage(output.isEmpty() ? QStringLiteral("rigctld did not open its TCP port")
                                        : QStringLiteral("rigctld failed: %1").arg(output));
    rigctldProcess_.terminate();
    rigctldProcess_.waitForFinished(1000);
    if (rigctldProcess_.state() != QProcess::NotRunning)
        rigctldProcess_.kill();
    rigctldHost_.clear();
    rigctldPort_ = 0;
    rigctldManaged_ = false;
    return false;
}

void CatController::connectRig(int modelId,
                               const QString &devicePath,
                               int serialSpeed,
                               CatSerialLineState rtsState,
                               CatSerialLineState dtrState,
                               CatPttMethod pttMethod,
                               int debugLevel)
{
#if MERCURYCHAT_WITH_HAMLIB
    disconnectRig();

    rig_set_debug(static_cast<enum rig_debug_level_e>(debugLevel));
    rig_ = rig_init(modelId);
    if (!rig_)
    {
        emit statusMessage(QStringLiteral("hamlib could not initialize rig model %1").arg(modelId));
        emit connectedChanged(false);
        return;
    }

    const QByteArray deviceBytes = devicePath.trimmed().toLocal8Bit();
    if (!deviceBytes.isEmpty())
    {
        std::snprintf(rig_->state.rigport.pathname, HAMLIB_FILPATHLEN, "%s", deviceBytes.constData());
        std::snprintf(rig_->state.pttport.pathname, HAMLIB_FILPATHLEN, "%s", deviceBytes.constData());
    }

    if (serialSpeed > 0)
    {
        rig_->state.rigport.parm.serial.rate = serialSpeed;
        rig_->state.pttport.parm.serial.rate = serialSpeed;
    }

    rig_->state.rigport.parm.serial.rts_state = toHamlibSerialState(rtsState);
    rig_->state.rigport.parm.serial.dtr_state = toHamlibSerialState(dtrState);
    rig_->state.pttport.parm.serial.rts_state = toHamlibSerialState(rtsState);
    rig_->state.pttport.parm.serial.dtr_state = toHamlibSerialState(dtrState);

    switch (pttMethod)
    {
    case CatPttMethod::SerialRts:
        rig_->state.pttport.type.ptt = RIG_PTT_SERIAL_RTS;
        break;
    case CatPttMethod::SerialDtr:
        rig_->state.pttport.type.ptt = RIG_PTT_SERIAL_DTR;
        break;
    case CatPttMethod::Cat:
        rig_->state.pttport.type.ptt = RIG_PTT_RIG;
        break;
    }

    const int rc = rig_open(rig_);
    if (rc != RIG_OK)
    {
        emit statusMessage(QStringLiteral("hamlib rig_open failed: %1").arg(QString::fromLocal8Bit(rigerror(rc))));
        rig_cleanup(rig_);
        rig_ = nullptr;
        emit connectedChanged(false);
        return;
    }

    connected_ = true;
    emit connectedChanged(true);
    emit statusMessage(QStringLiteral("hamlib CAT connected to %1").arg(rigDisplayName(rig_->caps)));
    refreshFrequency();
#else
    Q_UNUSED(debugLevel)
    disconnectRig();

    QString host;
    quint16 port = 0;
    if (parseHostPort(devicePath, &host, &port))
    {
        rigctldHost_ = host;
        rigctldPort_ = port;
        rigctldManaged_ = false;
        if (!connectToRigctld(rigctldHost_, rigctldPort_))
        {
            emit statusMessage(QStringLiteral("could not connect to rigctld at %1:%2").arg(host).arg(port));
            rigctldHost_.clear();
            rigctldPort_ = 0;
            emit connectedChanged(false);
            return;
        }
    }
    else if (!startBundledRigctld(modelId, devicePath, serialSpeed, rtsState, dtrState, pttMethod))
    {
        emit connectedChanged(false);
        return;
    }

    connected_ = true;
    emit connectedChanged(true);
    emit statusMessage(rigctldManaged_ ? QStringLiteral("hamlib CAT connected through bundled rigctld")
                                       : QStringLiteral("hamlib CAT connected to external rigctld"));
    refreshFrequency();
#endif
}

void CatController::disconnectRig()
{
#if MERCURYCHAT_WITH_HAMLIB
    if (rig_)
    {
        rig_set_ptt(rig_, RIG_VFO_CURR, RIG_PTT_OFF);
        rig_close(rig_);
        rig_cleanup(rig_);
        rig_ = nullptr;
    }
#endif

    if (rigctldManaged_ && rigctldProcess_.state() != QProcess::NotRunning)
    {
        bool ok = false;
        sendRigctldCommand(QStringLiteral("q"), &ok, 1000);
        rigctldProcess_.terminate();
        rigctldProcess_.waitForFinished(1500);
        if (rigctldProcess_.state() != QProcess::NotRunning)
        {
            rigctldProcess_.kill();
            rigctldProcess_.waitForFinished();
        }
    }

    rigctldHost_.clear();
    rigctldPort_ = 0;
    rigctldManaged_ = false;

    if (connected_)
    {
        connected_ = false;
        emit connectedChanged(false);
        emit statusMessage(QStringLiteral("hamlib CAT disconnected"));
    }
}

void CatController::refreshFrequency()
{
#if MERCURYCHAT_WITH_HAMLIB
    if (!rig_)
    {
        emit statusMessage(QStringLiteral("CAT is not connected"));
        return;
    }

    freq_t frequency = 0;
    const int rc = rig_get_freq(rig_, RIG_VFO_CURR, &frequency);
    if (rc != RIG_OK)
    {
        emit statusMessage(QStringLiteral("hamlib get frequency failed: %1").arg(QString::fromLocal8Bit(rigerror(rc))));
        return;
    }

    emit frequencyChanged(static_cast<qint64>(frequency));
#else
    if (!connected_)
    {
        emit statusMessage(QStringLiteral("CAT is not connected"));
        return;
    }

    bool ok = false;
    const QString value = firstValueLine(sendRigctldCommand(QStringLiteral("f"), &ok));
    bool numberOk = false;
    const qint64 frequency = static_cast<qint64>(value.toDouble(&numberOk));
    if (!ok || !numberOk)
    {
        emit statusMessage(QStringLiteral("hamlib get frequency failed"));
        return;
    }

    emit frequencyChanged(frequency);
#endif
}

void CatController::setFrequencyHz(qint64 frequencyHz)
{
#if MERCURYCHAT_WITH_HAMLIB
    if (!rig_)
    {
        emit statusMessage(QStringLiteral("CAT is not connected"));
        return;
    }

    const int rc = rig_set_freq(rig_, RIG_VFO_CURR, static_cast<freq_t>(frequencyHz));
    if (rc != RIG_OK)
    {
        emit statusMessage(QStringLiteral("hamlib set frequency failed: %1").arg(QString::fromLocal8Bit(rigerror(rc))));
        return;
    }

    emit frequencyChanged(frequencyHz);
#else
    if (!connected_)
    {
        emit statusMessage(QStringLiteral("CAT is not connected"));
        return;
    }

    bool ok = false;
    sendRigctldCommand(QStringLiteral("F %1").arg(frequencyHz), &ok);
    if (!ok)
    {
        emit statusMessage(QStringLiteral("hamlib set frequency failed"));
        return;
    }

    emit frequencyChanged(frequencyHz);
#endif
}

void CatController::setPtt(bool enabled)
{
#if MERCURYCHAT_WITH_HAMLIB
    if (!rig_)
    {
        emit statusMessage(QStringLiteral("CAT is not connected"));
        return;
    }

    const int rc = rig_set_ptt(rig_, RIG_VFO_CURR, enabled ? RIG_PTT_ON : RIG_PTT_OFF);
    if (rc != RIG_OK)
    {
        emit statusMessage(QStringLiteral("hamlib PTT failed: %1").arg(QString::fromLocal8Bit(rigerror(rc))));
        return;
    }

    emit pttChanged(enabled);
#else
    if (!connected_)
    {
        emit statusMessage(QStringLiteral("CAT is not connected"));
        return;
    }

    bool ok = false;
    sendRigctldCommand(QStringLiteral("T %1").arg(enabled ? 1 : 0), &ok);
    if (!ok)
    {
        emit statusMessage(QStringLiteral("hamlib PTT failed"));
        return;
    }

    emit pttChanged(enabled);
#endif
}
