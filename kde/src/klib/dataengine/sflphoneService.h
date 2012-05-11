#ifndef SFLPHONE_SERVICE_H
#define SFLPHONE_SERVICE_H

#include "sflphonEngine.h"

#include <Plasma/Service>
#include <Plasma/ServiceJob>

#include "../../lib/Call.h"
#include "../../lib/CallModel.h"

using namespace Plasma;

class SFLPhoneService : public Plasma::Service
{
    Q_OBJECT

public:
    SFLPhoneService(SFLPhoneEngine *engine);
    ServiceJob *createJob(const QString &operation,
                          QMap<QString, QVariant> &parameters);

private:
    SFLPhoneEngine *m_engine;

};

class CallJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
    CallJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap())
        : Plasma::ServiceJob("", operation, parameters, parent)
        , m_AccountId ( parameters["AccountId"].toString() )
        , m_Number    ( parameters["Number"].toString()    )
    {}

    void start()
    {
      qDebug() << "TRYING TO CALL USING" << m_AccountId << m_Number;
//       Call* call = SFLPhoneEngine::getModel()->addDialingCall("112",m_AccountId);
//       call->setCallNumber(m_Number);
//       call->actionPerformed(CALL_ACTION_ACCEPT);
    }

private:
    QString m_AccountId;
    QString m_Number;
};

#endif //SFLPHONE_SERVICE_H
