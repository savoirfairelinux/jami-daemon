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
    ServiceJob *createJob(const QString &operation, QMap<QString, QVariant> &parameters);

private:
    SFLPhoneEngine *m_engine;

};

class CallJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
    CallJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap())
        : Plasma::ServiceJob("", operation, parameters, parent)
        , m_AccountId ( parameters[ "AccountId" ].toString() )
        , m_Number    ( parameters[ "Number"    ].toString() )
    {}

    void start()
    {
      Call* call = SFLPhoneEngine::getModel()->addDialingCall(m_Number,m_AccountId);
      call->setCallNumber(m_Number);
      call->actionPerformed(CALL_ACTION_ACCEPT);
    }

private:
    QString m_AccountId;
    QString m_Number;
};

class DTMFJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
    DTMFJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap())
        : Plasma::ServiceJob("", operation, parameters, parent)
        , m_mStr( parameters[ "str" ].toString() )
    {}

   void start()
   {
      CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
      callManager.playDTMF(m_mStr);
   }
private:
   QString m_mStr;
};

class HangUpJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
    HangUpJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap())
        : Plasma::ServiceJob("", operation, parameters, parent)
        , m_CallId( parameters[ "callid" ].toString() )
    {}

   void start()
   {
      Call* call = SFLPhoneEngine::getModel()->getCall(m_CallId);
      call->actionPerformed(CALL_ACTION_REFUSE);
      call->changeCurrentState(CALL_STATE_OVER);
   }
private:
   QString m_CallId;
};

class TransferJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
    TransferJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap())
        : Plasma::ServiceJob("", operation, parameters, parent)
        , m_CallId         ( parameters[ "callid" ].toString()         )
        , m_transferNumber ( parameters[ "transfernumber" ].toString() )
    {}

   void start()
   {
      Call* call = SFLPhoneEngine::getModel()->getCall(m_CallId);
      call->setTransferNumber(m_transferNumber);
      call->changeCurrentState(CALL_STATE_TRANSFER);
      call->actionPerformed(CALL_ACTION_ACCEPT);
      call->changeCurrentState(CALL_STATE_OVER);
   }
private:
   QString m_CallId;
   QString m_transferNumber;
};

class HoldJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
    HoldJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap())
        : Plasma::ServiceJob("", operation, parameters, parent)
        , m_CallId         ( parameters[ "callid" ].toString() )
    {}

   void start()
   {
      Call* call = SFLPhoneEngine::getModel()->getCall(m_CallId);
      call->actionPerformed(CALL_ACTION_HOLD);
   }
private:
   QString m_CallId;
};

class RecordJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
    RecordJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap())
        : Plasma::ServiceJob("", operation, parameters, parent)
        , m_CallId         ( parameters[ "callid" ].toString() )
    {}

   void start()
   {
      Call* call = SFLPhoneEngine::getModel()->getCall(m_CallId);
      call->actionPerformed(CALL_ACTION_RECORD);
   }
private:
   QString m_CallId;
};


#endif //SFLPHONE_SERVICE_H
