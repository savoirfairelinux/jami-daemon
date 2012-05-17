#include "sflphoneService.h"

#include "../../lib/Call.h"

SFLPhoneService::SFLPhoneService(SFLPhoneEngine *engine)

{
    m_engine = engine;
    setName("sflphone");
}

ServiceJob *SFLPhoneService::createJob(const QString &operation, QMap<QString, QVariant> &parameters)
{
    if (!m_engine) {
        return 0;
    }

    if      (operation == "Call") {
       return new CallJob(this, operation,parameters);
    }
    else if (operation == "DMTF") {
       return new DTMFJob(this, operation,parameters);
    }
    else if (operation == "Transfer") {
       return new TransferJob(this, operation,parameters);
    }
    else if (operation == "Hangup") {
       return new HangUpJob(this, operation,parameters);
    }
    else if (operation == "Hold") {
       return new HoldJob(this, operation,parameters);
    }
    else if (operation == "Record") {
       return new RecordJob(this, operation,parameters);
    }
    m_engine->setData(operation, parameters["query"]);
    return 0;
}