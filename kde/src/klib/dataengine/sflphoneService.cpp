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