#include "CatController.hpp"

#include <QByteArray>

#if MERCURYCHAT_WITH_HAMLIB
#include <algorithm>
#include <cstdio>
#endif

#if MERCURYCHAT_WITH_HAMLIB
namespace
{
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
}
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
#endif

    return models;
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
    Q_UNUSED(modelId)
    Q_UNUSED(devicePath)
    Q_UNUSED(serialSpeed)
    Q_UNUSED(rtsState)
    Q_UNUSED(dtrState)
    Q_UNUSED(pttMethod)
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
