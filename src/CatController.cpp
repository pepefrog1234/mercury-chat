#include "CatController.hpp"

#include <QByteArray>

#if MERCURYCHAT_WITH_HAMLIB
#include <cstdio>
#endif

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

void CatController::connectRig(int modelId, const QString &devicePath, int serialSpeed, int debugLevel)
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
        std::snprintf(rig_->state.rigport.pathname, HAMLIB_FILPATHLEN, "%s", deviceBytes.constData());

    if (serialSpeed > 0)
        rig_->state.rigport.parm.serial.rate = serialSpeed;

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
    emit statusMessage(QStringLiteral("hamlib CAT connected to model %1").arg(modelId));
    refreshFrequency();
#else
    Q_UNUSED(modelId)
    Q_UNUSED(devicePath)
    Q_UNUSED(serialSpeed)
    Q_UNUSED(debugLevel)
    emit statusMessage(QStringLiteral("This build was configured without hamlib CAT support"));
    emit connectedChanged(false);
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
    emit statusMessage(QStringLiteral("This build was configured without hamlib CAT support"));
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
    Q_UNUSED(frequencyHz)
    emit statusMessage(QStringLiteral("This build was configured without hamlib CAT support"));
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
    Q_UNUSED(enabled)
    emit statusMessage(QStringLiteral("This build was configured without hamlib CAT support"));
#endif
}

