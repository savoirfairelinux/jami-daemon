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

#ifndef RING_PROTOCOL_H
#define RING_PROTOCOL_H

#include <TelepathyQt/BaseProtocol>

class Protocol : public Tp::BaseProtocol
{
    Q_OBJECT
    Q_DISABLE_COPY(Protocol)

public:
    Protocol(const QDBusConnection &dbusConnection, const QString &name);

private:
    Tp::BaseConnectionPtr createConnection(const QVariantMap &parameters, Tp::DBusError *error);
};

#endif
