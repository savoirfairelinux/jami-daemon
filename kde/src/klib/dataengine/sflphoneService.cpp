/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

#include "sflphoneService.h"

#include "../../lib/Call.h"
#include "../../lib/Account.h"

/*****************************************************************************
 *                                                                           *
 *                                    Jobs                                   *
 *                                                                           *
 ****************************************************************************/

///Constructor
CallJob::CallJob(QObject* parent, const QString& operation, const QVariantMap& parameters)
   : Plasma::ServiceJob("", operation, parameters, parent)
   , m_pAccount ( Account::buildExistingAccountFromId(parameters[ "AccountId" ].toString() ))
   , m_Number   ( parameters[ "Number"    ].toString() )
{
   
}

///Make a call
void CallJob::start()
{
   Call* call = SFLPhoneEngine::getModel()->addDialingCall(m_Number,m_pAccount);
   call->setCallNumber(m_Number);
   call->actionPerformed(CALL_ACTION_ACCEPT);
}


///Constructor
DTMFJob::DTMFJob(QObject* parent, const QString& operation, const QVariantMap& parameters)
   : Plasma::ServiceJob("", operation, parameters, parent)
   , m_mStr( parameters[ "str" ].toString() )
{
   
}

///Play a DMTF tone
void DTMFJob::start()
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   callManager.playDTMF(m_mStr);
}


///Constructor
HangUpJob::HangUpJob(QObject* parent, const QString& operation, const QVariantMap& parameters)
   : Plasma::ServiceJob("", operation, parameters, parent)
   , m_CallId( parameters[ "callid" ].toString() )
{
   
}

///Hang up a call
void HangUpJob::start()
{
   Call* call = SFLPhoneEngine::getModel()->getCall(m_CallId);
   call->actionPerformed(CALL_ACTION_REFUSE);
   call->changeCurrentState(CALL_STATE_OVER);
}


///Constructor
TransferJob::TransferJob(QObject* parent, const QString& operation, const QVariantMap& parameters)
   : Plasma::ServiceJob("", operation, parameters, parent)
   , m_CallId         ( parameters[ "callid" ].toString()         )
   , m_transferNumber ( parameters[ "transfernumber" ].toString() )
{
   
}


///Tranfer a call
void TransferJob::start()
{
   Call* call = SFLPhoneEngine::getModel()->getCall(m_CallId);
   call->setTransferNumber(m_transferNumber);
   call->changeCurrentState(CALL_STATE_TRANSFER);
   call->actionPerformed(CALL_ACTION_ACCEPT);
   call->changeCurrentState(CALL_STATE_OVER);
}

///Constructor
HoldJob::HoldJob(QObject* parent, const QString& operation, const QVariantMap& parameters)
   : Plasma::ServiceJob("", operation, parameters, parent)
   , m_CallId         ( parameters[ "callid" ].toString() )
{}


///Put a call on hold
void HoldJob::start()
{
   Call* call = SFLPhoneEngine::getModel()->getCall(m_CallId);
   call->actionPerformed(CALL_ACTION_HOLD);
}

///Constructor
RecordJob::RecordJob(QObject* parent, const QString& operation, const QVariantMap& parameters)
   : Plasma::ServiceJob("", operation, parameters, parent)
   , m_CallId         ( parameters[ "callid" ].toString() )
{
   
}


///Record a call
void RecordJob::start()
{
   Call* call = SFLPhoneEngine::getModel()->getCall(m_CallId);
   call->actionPerformed(CALL_ACTION_RECORD);
}


/*****************************************************************************
 *                                                                           *
 *                        Service related functions                          *
 *                                                                           *
 ****************************************************************************/

///Constructor
SFLPhoneService::SFLPhoneService(SFLPhoneEngine *engine):m_engine(engine)
{
    setName("sflphone");
}

///Constructor
ServiceJob *SFLPhoneService::createJob(const QString &operation, QMap<QString, QVariant> &parameters)
{
   if (!m_engine)
      return 0;

   /*                   RPC NAME                                   JOB                          */
   /**/if      (operation == "Call"     ) { return new CallJob    ( this, operation,parameters); }
   /**/else if (operation == "DMTF"     ) { return new DTMFJob    ( this, operation,parameters); }
   /**/else if (operation == "Transfer" ) { return new TransferJob( this, operation,parameters); }
   /**/else if (operation == "Hangup"   ) { return new HangUpJob  ( this, operation,parameters); }
   /**/else if (operation == "Hold"     ) { return new HoldJob    ( this, operation,parameters); }
   /**/else if (operation == "Record"   ) { return new RecordJob  ( this, operation,parameters); }
   /*                                                                                           */

   m_engine->setData(operation, parameters["query"]);
   return 0;
}