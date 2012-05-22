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

#ifndef SFLPHONE_SERVICE_H
#define SFLPHONE_SERVICE_H

#include "sflphonEngine.h"

#include <Plasma/Service>
#include <Plasma/ServiceJob>

#include "../../lib/Call.h"
#include "../../lib/CallModel.h"

using namespace Plasma;

///@class SFLPhoneService RPC between the plasmoid and the library
class SFLPhoneService : public Plasma::Service
{
   Q_OBJECT

public:
   SFLPhoneService(SFLPhoneEngine *engine);
   ServiceJob *createJob(const QString &operation, QMap<QString, QVariant> &parameters);

private:
   SFLPhoneEngine *m_engine;
};

///@class CallJob Call using and account and a number
class CallJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
   CallJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap());

   void start();

private:
    QString m_AccountId;
    QString m_Number   ;
};

///@class DTMFJob Play a sound when called
class DTMFJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
   DTMFJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap());

   void start();
private:
   QString m_mStr;
};

///@class HangUpJob Hang up a call using an id
class HangUpJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
   HangUpJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap());

   void start();
private:
   QString m_CallId;
};

///@class TransferJob Transfer a call
class TransferJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
   TransferJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap());

   void start();
private:
   QString m_CallId        ;
   QString m_transferNumber;
};

///@class HoldJob Put or remove a call from hold mode
class HoldJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
   HoldJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap());

   void start();
private:
   QString m_CallId;
};

///@class RecordJob Record a call
class RecordJob : public Plasma::ServiceJob
{
   Q_OBJECT
public:
   RecordJob(QObject* parent, const QString& operation, const QVariantMap& parameters = QVariantMap());

   void start();
private:
   QString m_CallId;
};


#endif //SFLPHONE_SERVICE_H
