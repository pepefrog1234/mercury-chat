#pragma once

#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>

#if MERCURYCHAT_WITH_HAMLIB
#include <hamlib/rig.h>
#endif

struct CatRigModel
{
    int modelId = 0;
    QString name;
};

enum class CatSerialLineState
{
    Unset = 0,
    On = 1,
    Off = 2
};

enum class CatPttMethod
{
    Cat = 0,
    SerialRts = 1,
    SerialDtr = 2
};

class CatController : public QObject
{
    Q_OBJECT

public:
    explicit CatController(QObject *parent = nullptr);
    ~CatController() override;

    static QList<CatRigModel> availableRigModels();
    bool isConnected() const;

public slots:
    void connectRig(int modelId,
                    const QString &devicePath,
                    int serialSpeed,
                    CatSerialLineState rtsState = CatSerialLineState::Unset,
                    CatSerialLineState dtrState = CatSerialLineState::Unset,
                    CatPttMethod pttMethod = CatPttMethod::Cat,
                    int debugLevel = 0);
    void disconnectRig();
    void refreshFrequency();
    void setFrequencyHz(qint64 frequencyHz);
    void setPtt(bool enabled);

signals:
    void connectedChanged(bool connected);
    void statusMessage(const QString &message);
    void frequencyChanged(qint64 frequencyHz);
    void pttChanged(bool enabled);

private:
    static QString findHamlibTool(const QString &toolName);
    static bool parseHostPort(const QString &text, QString *host, quint16 *port);
    static QString pttTypeName(CatPttMethod pttMethod);
    static QString serialLineStateName(CatSerialLineState state);

    bool connectToRigctld(const QString &host, quint16 port, int timeoutMs = 3000) const;
    QStringList sendRigctldCommand(const QString &command, bool *ok = nullptr, int timeoutMs = 3000) const;
    bool startBundledRigctld(int modelId,
                             const QString &devicePath,
                             int serialSpeed,
                             CatSerialLineState rtsState,
                             CatSerialLineState dtrState,
                             CatPttMethod pttMethod);

#if MERCURYCHAT_WITH_HAMLIB
    RIG *rig_ = nullptr;
#endif
    QProcess rigctldProcess_;
    QString rigctldHost_;
    quint16 rigctldPort_ = 0;
    bool rigctldManaged_ = false;
    bool connected_ = false;
};
