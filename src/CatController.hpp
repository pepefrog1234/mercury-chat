#pragma once

#include <QObject>
#include <QString>

#if MERCURYCHAT_WITH_HAMLIB
#include <hamlib/rig.h>
#endif

class CatController : public QObject
{
    Q_OBJECT

public:
    explicit CatController(QObject *parent = nullptr);
    ~CatController() override;

    bool isConnected() const;

public slots:
    void connectRig(int modelId, const QString &devicePath, int serialSpeed, int debugLevel = 0);
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
#if MERCURYCHAT_WITH_HAMLIB
    RIG *rig_ = nullptr;
#endif
    bool connected_ = false;
};

