/**
 *  Ring Protocol Telepathy Connection Manager
 *
 * Copyright (C) 2016 Kevin Avignon, All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "protocol.h"
#include "connection.h"

#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/ProtocolParameterList>

Protocol::Protocol(const QDBusConnection &dbusConnection, const QString &name)
    : Tp::BaseProtocol(dbusConnection, name)
{
    setRequestableChannelClasses(Tp::RequestableChannelClassSpecList() <<
                                 Tp::RequestableChannelClassSpec::textChat());

    setCreateConnectionCallback(memFun(this, &Protocol::createConnection));
    Tp::ProtocolParameterList parameters;
    Tp::ProtocolParameter parameterAccount("account", "s", Tp::ConnMgrParamFlagRequired);
    Tp::ProtocolParameter parameterPassword("password", "s", Tp::ConnMgrParamFlagRequired|Tp::ConnMgrParamFlagSecret);
    parameters << parameterAccount << parameterPassword;

    setParameters(parameters);
}

Tp::BaseConnectionPtr Protocol::createConnection(const QVariantMap &parameters, Tp::DBusError *error)
{
    Q_UNUSED(error);
    return Tp::BaseConnection::create<HangingConnection>("hangouts", name().toLatin1(), parameters);
}
