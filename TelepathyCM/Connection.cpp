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

#include <QDebug>
#include <QStandardPaths>

#include <TelepathyQt/Constants>
#include <TelepathyQt/BaseChannel>
#include <TelepathyQt/DBusObject>
#include <QNetworkReply>
#include <QCryptographicHash>

#include "Connection.h"
#include "ProtocolSession.h"

RingConnection::RingConnection      (const QDBusConnection &dbusConnection, const QString &cmName,
                                     const QString &protocolName,
                                     const QVariantMap &parameters) :
    Tp::BaseConnection(dbusConnection, cmName, protocolName, parameters),
    mLastKnownUpdate(0),
    mHandleCount(0),
    mHangishClient(NULL),
    mDisconnectReason(Tp::ConnectionStatusReasonRequested)
{
    mAccount = parameters["account"].toString();
    mPassword = parameters["password"].toString();
    mConfigurationDirectory = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/" + mAccount + "/";
    mTokenPath = mConfigurationDirectory + "cookies.json";

    // ensure cache dir is created
    QDir dir;
    if (!dir.mkpath(mConfigurationDirectory)) {
        qWarning() << "Failed to create cache directory";
    }

    mSettings = new QSettings(mConfigurationDirectory+"config.ini", QSettings::NativeFormat);

    setConnectCallback(Tp::memFun(this,&RingConnection::connect));
    setInspectHandlesCallback(Tp::memFun(this,&RingConnection::inspectHandles));
    setRequestHandlesCallback(Tp::memFun(this,&RingConnection::requestHandles));
    setCreateChannelCallback(Tp::memFun(this,&RingConnection::createChannel));

    // initialise requests interface (Connection.Interface.Requests)
    requestsIface = Tp::BaseConnectionRequestsInterface::create(this);

    // set requestable text channel properties
    Tp::RequestableChannelClass text;
    text.fixedProperties[TP_QT_IFACE_CHANNEL+".ChannelType"] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    text.fixedProperties[TP_QT_IFACE_CHANNEL+".TargetHandleType"]  = Tp::HandleTypeContact;
    text.allowedProperties.append(TP_QT_IFACE_CHANNEL+".TargetHandle");
    text.allowedProperties.append(TP_QT_IFACE_CHANNEL+".TargetID");

    Tp::RequestableChannelClass groupChat;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    groupChat.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeRoom;
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"));
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"));
    groupChat.allowedProperties.append(TP_QT_IFACE_CHANNEL_INTERFACE_CONFERENCE + QLatin1String(".InitialInviteeHandles"));

    Tp::RequestableChannelClass chatList;
    chatList.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_ROOM_LIST;
    chatList.fixedProperties[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")]  = Tp::HandleTypeNone;

    requestsIface->requestableChannelClasses << text << groupChat << chatList;

    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(requestsIface));

    // init presence interface
    simplePresenceIface = Tp::BaseConnectionSimplePresenceInterface::create();
    simplePresenceIface->setSetPresenceCallback(Tp::memFun(this,&RingConnection::setPresence));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(simplePresenceIface));

    // Set Presences
    Tp::SimpleStatusSpec presenceOnline;
    presenceOnline.type = Tp::ConnectionPresenceTypeAvailable;
    presenceOnline.maySetOnSelf = true;
    presenceOnline.canHaveMessage = false;

    Tp::SimpleStatusSpec presenceHidden;
    presenceHidden.type = Tp::ConnectionPresenceTypeHidden;
    presenceHidden.maySetOnSelf = true;
    presenceHidden.canHaveMessage = false;

    Tp::SimpleStatusSpec presenceOffline;
    presenceOffline.type = Tp::ConnectionPresenceTypeOffline;
    presenceOffline.maySetOnSelf = true;
    presenceOffline.canHaveMessage = false;

    Tp::SimpleStatusSpec presenceUnknown;
    presenceUnknown.type = Tp::ConnectionPresenceTypeUnknown;
    presenceUnknown.maySetOnSelf = false;
    presenceUnknown.canHaveMessage = false;

    Tp::SimpleStatusSpecMap statuses;
    statuses.insert(QLatin1String("available"), presenceOnline);
    statuses.insert(QLatin1String("offline"), presenceOffline);
    statuses.insert(QLatin1String("hidden"), presenceHidden);
    statuses.insert(QLatin1String("unknown"), presenceUnknown);

    simplePresenceIface->setStatuses(statuses);

    contactsIface = Tp::BaseConnectionContactsInterface::create();
    contactsIface->setGetContactAttributesCallback(Tp::memFun(this,&RingConnection::getContactAttributes));
    contactsIface->setContactAttributeInterfaces(QStringList()
            << TP_QT_IFACE_CONNECTION
            << TP_QT_IFACE_CONNECTION_INTERFACE_CONTACT_LIST
            << TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING
            << TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE
            << TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS);
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactsIface));

    /* Connection.Interface.Avatars */
    avatarsIface = Tp::BaseConnectionAvatarsInterface::create();
    avatarsIface->setAvatarDetails(Tp::AvatarSpec(/* supportedMimeTypes */ QStringList() << QLatin1String("image/jpeg"),
                                   /* minHeight */ 0, /* maxHeight */ 160, /* recommendedHeight */ 160,
                                   /* minWidth */ 0, /* maxWidth */ 160, /* recommendedWidth */ 160,
                                   /* maxBytes */ 10240));
    avatarsIface->setGetKnownAvatarTokensCallback(Tp::memFun(this, &RingConnection::getKnownAvatarTokens));
    avatarsIface->setRequestAvatarsCallback(Tp::memFun(this, &RingConnection::requestAvatars));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(avatarsIface));


    contactListIface = Tp::BaseConnectionContactListInterface::create();
    contactListIface->setContactListPersists(true);
    //contactListIface->setCanChangeContactList(true);
    contactListIface->setDownloadAtConnection(true);
    contactListIface->setGetContactListAttributesCallback(Tp::memFun(this, &RingConnection::getContactListAttributes));

    // TODO: implement once libhangish supports these operations
    //contactListIface->setRequestSubscriptionCallback(Tp::memFun(this, &RingConnection::requestSubscription));
    //contactListIface->setRemoveContactsCallback(Tp::memFun(this, &RingConnection::removeContacts));
    plugInterface(Tp::AbstractConnectionInterfacePtr::dynamicCast(contactListIface));

    QObject::connect(this, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
}

void RingConnection::onDisconnected()
{
    // save last received event timestamp so next connection we just retrieve the delta.
    // also do not save properties if we are not connected yet, otherwise we will overwrite
    // conversationIds with an empty value
    if (mSettings) {
        if (mLastKnownUpdate != 0) {
            mSettings->setValue("lastKnownUpdate", mLastKnownUpdate);
        }
        if (!mConversations.isEmpty()) {
            mSettings->setValue("conversationIds", QString(QStringList(mConversations.keys()).join(",")));
        }
    }
    if (mHangishClient) {
        mHangishClient->hangishDisconnect();
        mHangishClient->deleteLater();
        mHangishClient = NULL;
    }
    setStatus(Tp::ConnectionStatusDisconnected, mDisconnectReason);
    deleteLater();
}

void RingConnection::connect(Tp::DBusError *error)
{
    Q_UNUSED(error)
    if (status() == Tp::ConnectionStatusDisconnected) {
        setStatus(Tp::ConnectionStatusConnecting, Tp::ConnectionStatusReasonRequested);
        mLastKnownUpdate = mSettings->value("lastKnownUpdate", 0).value<quint64>();

        mHangishClient = new HangishClient(mTokenPath);

        QObject::connect(mHangishClient, SIGNAL(initFinished()), this, SLOT(onInitFinished()));
        QObject::connect(mHangishClient, SIGNAL(authFailed(AuthenticationStatus,QString)), this, SLOT(onAuthFailed(AuthenticationStatus,QString)));
        QObject::connect(mHangishClient, SIGNAL(clientSetPresenceResponse(quint64,ClientSetPresenceResponse&)), this, SLOT(onClientSetPresenceResponse(quint64,ClientSetPresenceResponse&)));
        QObject::connect(mHangishClient, SIGNAL(loginNeeded()), this, SLOT(onLoginNeeded()));
        QObject::connect(mHangishClient, SIGNAL(clientStateUpdate(ClientStateUpdate&)), this, SLOT(onClientStateUpdate(ClientStateUpdate&)));
        QObject::connect(mHangishClient, SIGNAL(clientGetConversationResponse(quint64,ClientGetConversationResponse&)), this, SLOT(onClientGetConversationResponse(quint64,ClientGetConversationResponse&)));
        QObject::connect(mHangishClient, SIGNAL(clientSyncAllNewEventsResponse(ClientSyncAllNewEventsResponse&)), this, SLOT(onClientSyncAllNewEventsResponse(ClientSyncAllNewEventsResponse&)));
        QObject::connect(mHangishClient, SIGNAL(clientQueryPresenceResponse(quint64,ClientQueryPresenceResponse&)), this, SLOT(onClientQueryPresenceResponse(quint64,ClientQueryPresenceResponse&)));
        QObject::connect(mHangishClient, SIGNAL(connectionStatusChanged(ConnectionStatus)), this, SLOT(onConnectionStatusChanged(ConnectionStatus)));

        mHangishClient->hangishConnect(mLastKnownUpdate);
    }
}

void RingConnection::onConnectionStatusChanged(ConnectionStatus status)
{
    switch(status) {
    case CONNECTION_STATUS_DISCONNECTED:
        mDisconnectReason = Tp::ConnectionStatusReasonNetworkError;
        Q_EMIT disconnected();
        break;
    case CONNECTION_STATUS_CONNECTED:
        setStatus(Tp::ConnectionStatusConnected, Tp::ConnectionStatusReasonNoneSpecified);
        break;
    case CONNECTION_STATUS_CONNECTING:
        setStatus(Tp::ConnectionStatusConnecting, Tp::ConnectionStatusReasonNoneSpecified);
        break;
    }
}

void RingConnection::onAuthFailed(AuthenticationStatus status, QString reason)
{
    QVariantMap details;

    switch (status) {
    case AUTH_WRONG_2FACTOR_PIN:
        details[QLatin1String("server-message")] = reason;
        saslIface->setSaslStatus(Tp::SASLStatusServerFailed, TP_QT_ERROR_AUTHENTICATION_FAILED, details);
        setStatus(Tp::ConnectionStatusDisconnected, Tp::ConnectionStatusReasonAuthenticationFailed);
        break;
    case AUTH_WRONG_CREDENTIALS:
    case AUTH_CANT_GET_GALX_TOKEN:
    case AUTH_UNKNOWN_ERROR:
        setStatus(Tp::ConnectionStatusDisconnected, Tp::ConnectionStatusReasonAuthenticationFailed);
        break;
    case AUTH_NEED_2FACTOR_PIN:
        on2FactorAuthRequired();
        break;
    }
}

void RingConnection::onLoginNeeded()
{
    mHangishClient->sendCredentials(mAccount, mPassword);
}

void RingConnection::on2FactorAuthRequired()
{
    Tp::DBusError error;

    //Registration
    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, TP_QT_IFACE_CHANNEL_TYPE_SERVER_AUTHENTICATION);

    Tp::BaseChannelServerAuthenticationTypePtr authType
        = Tp::BaseChannelServerAuthenticationType::create(TP_QT_IFACE_CHANNEL_INTERFACE_SASL_AUTHENTICATION);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(authType));

    saslIface = Tp::BaseChannelSASLAuthenticationInterface::create(QStringList() << QLatin1String("X-TELEPATHY-PASSWORD"),
                /* hasInitialData */ false,
                /* canTryAgain */ true,
                /* authorizationIdentity */ mAccount,
                /* defaultUsername */ QString(),
                /* defaultRealm */ QString(),
                /* maySaveResponse */ false);

    saslIface->setStartMechanismWithDataCallback( Tp::memFun(this, &RingConnection::startMechanismWithData));

    baseChannel->setRequested(false);
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(saslIface));

    baseChannel->registerObject(&error);

    if (!error.isValid()) {
        addChannel(baseChannel);
    }
}

void RingConnection::startMechanismWithData(const QString &mechanism, const QByteArray &data, Tp::DBusError *error)
{
    Q_UNUSED(mechanism)
    Q_UNUSED(error)
    saslIface->setSaslStatus(Tp::SASLStatusInProgress, QLatin1String("InProgress"), QVariantMap());
    mHangishClient->sendChallengePin(QString::fromLatin1(data.constData()));
}

void RingConnection::requestAvatars(const Tp::UIntList &contacts, Tp::DBusError *error)
{
    const QStringList identifiers = inspectHandles(Tp::HandleTypeContact, contacts, error);

    if (error->isValid()) {
        return;
    }

    if (identifiers.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_HANDLE, QLatin1String("Invalid handle(s)"));
    }

    bool shouldStartFetching = mPendingAvatars.isEmpty();

    Q_FOREACH (const QString &identifier, identifiers) {
        ClientEntity entity = mEntities[identifier];
        QString url = QString("https:"+QString(entity.properties().photourl().c_str()));
        mPendingAvatars[identifier] = url;
    }

    if (shouldStartFetching) {
        QNetworkReply *reply = mNetworkManager.get(QNetworkRequest(mPendingAvatars.first()));
        QObject::connect(reply, SIGNAL(finished()), this, SLOT(onAvatarDownloaded()));
    }
}

void RingConnection::onAvatarDownloaded()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString url = reply->url().toString();

    if (!mPendingAvatars.key(url).isEmpty()) {
        QString identifier = mPendingAvatars.key(url);
        avatarsIface->avatarRetrieved(ensureContactHandle(identifier), url.toLatin1().toBase64(), reply->readAll(), "image/jpeg");
        mPendingAvatars.remove(identifier);
    }

    // we download one by one, so take the next one if there is more
    if (!mPendingAvatars.isEmpty()) {
        QNetworkReply *reply = mNetworkManager.get(QNetworkRequest(mPendingAvatars.first()));
        QObject::connect(reply, SIGNAL(finished()), this, SLOT(onAvatarDownloaded()));
    }
}

Tp::AvatarTokenMap RingConnection::getKnownAvatarTokens(const Tp::UIntList &contacts, Tp::DBusError *error)
{
    const QStringList identifiers = inspectHandles(Tp::HandleTypeContact, contacts, error);

    if (error->isValid()) {
        return Tp::AvatarTokenMap();
    }

    if (identifiers.isEmpty()) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, QLatin1String("Invalid handle(s)"));
        return Tp::AvatarTokenMap();
    }

    Tp::AvatarTokenMap result;
    for (int i = 0; i < contacts.count(); ++i) {
        ClientEntity entity = mEntities[identifiers.at(i)];
        if (!entity.has_properties() || !entity.properties().has_photourl()) {
            continue;
        }
        QString token(QByteArray(entity.properties().photourl().c_str()).toBase64());
        result.insert(contacts.at(i), token);
    }

    return result;
}

Tp::ContactAttributesMap RingConnection::getContactListAttributes(const QStringList &interfaces, bool hold, Tp::DBusError *error)
{
    Q_UNUSED(hold);

    Tp::UIntList handles = mContactHandles.keys();
    handles.removeOne(selfHandle());

    return getContactAttributes(handles, interfaces, error);
}

void RingConnection::onClientSetPresenceResponse(quint64 requestId, ClientSetPresenceResponse &cspr)
{
    Q_UNUSED(requestId)
    if (cspr.responseheader().has_status() && cspr.responseheader().status() == ClientResponseHeader_ClientResponseStatus_OK) {
        mRequestedPresence.clear();
    }
}

void RingConnection::onInitFinished()
{
    if (!saslIface.isNull()) {
        saslIface->setSaslStatus(Tp::SASLStatusSucceeded, QLatin1String("Succeeded"), QVariantMap());
    }

    if (!mRequestedPresence.isEmpty()) {
        mHangishClient->setPresence(mRequestedPresence == "available");
    }

    if (!mHangishClient->getSelfChatId().isEmpty()) {
        setSelfHandle(ensureContactHandle(mHangishClient->getSelfChatId()));
        setSelfID(mHangishClient->getSelfChatId());
    }

    contactListIface->setContactListState(Tp::ContactListStateWaiting);
    QList<uint> handles;

    mEntities = mHangishClient->getUsers();
    QMap<QString, ClientEntity>::iterator i;
    QStringList chatIds;
    for (i = mEntities.begin(); i != mEntities.end(); ++i) {
        uint handle = ensureContactHandle(i.value().id().chatid().c_str());
        if (handle == selfHandle()) {
            continue;
        }
        handles << handle;
        chatIds << i.key();
    }

    Tp::ContactSubscriptionMap changes;
    Tp::HandleIdentifierMap identifiersMap;
    for (int i = 0; i < chatIds.size(); ++i) {
        Tp::ContactSubscriptions change;
        change.publish = Tp::SubscriptionStateYes;
        change.publishRequest = QString();
        change.subscribe = Tp::SubscriptionStateYes;
        changes[handles[i]] = change;
        identifiersMap[handles[i]] = chatIds[i];
    }

    Tp::HandleIdentifierMap removals;
    contactListIface->contactsChangedWithID(changes, identifiersMap, removals);
    Tp::SimpleContactPresences newPresences;
    for (int i = 0; i < chatIds.size(); ++i) {
        QString chatId = chatIds[i];
        uint handle = ensureContactHandle(chatId);
        Tp::SimplePresence presence;
        presence.type = Tp::ConnectionPresenceTypeOffline;
        presence.status = "offline";
        ClientEntity entity = mEntities[chatId];
        if (entity.presence().available()) {
            presence.type = Tp::ConnectionPresenceTypeAvailable;
            presence.status = "available";
        }
        newPresences[handle] = presence;
    }
    mEntities[mHangishClient->getSelfChatId()] = mHangishClient->getMyself();
    simplePresenceIface->setPresences(newPresences);
    contactListIface->setContactListState(Tp::ContactListStateSuccess);
}

HangishClient *RingConnection::hangishClient()
{
    return mHangishClient;
}

HangingTextChannel* RingConnection::textChannelForConversationId(const QString &conversationId)
{
    Q_FOREACH(HangingTextChannel* channel, mTextChannels) {
        if (conversationId == channel->conversationId()) {
            return channel;
        }
    }
    return NULL;
}

RingConnection::~RingConnection()
{
    dbusConnection().unregisterObject(objectPath(), QDBusConnection::UnregisterTree);
    dbusConnection().unregisterService(busName());
    if (mHangishClient) {
        mHangishClient->hangishDisconnect();
        mHangishClient->deleteLater();
        mHangishClient = NULL;
    }
    if (mSettings) {
        mSettings->deleteLater();
    }
}

uint RingConnection::setPresence(const QString& newStatus, const QString& statusMessage, Tp::DBusError *error)
{
    Q_UNUSED(error)
    Q_UNUSED(statusMessage)
    if (status() != Tp::ConnectionStatusConnected) {
        mRequestedPresence = newStatus;
    } else {
        mHangishClient->setPresence(newStatus == "available");
    }
    return selfHandle();
}

Tp::ContactAttributesMap RingConnection::getContactAttributes(const Tp::UIntList &handles, const QStringList &ifaces, Tp::DBusError *error)
{
    qDebug() << "getContactAttributes" << handles << ifaces;
    Tp::ContactAttributesMap attributesMap;
    Q_FOREACH(uint handle, handles) {
        QVariantMap attributes;
        QStringList inspectedHandles = inspectHandles(Tp::HandleTypeContact, Tp::UIntList() << handle, error);
        QString chatId;
        if (inspectedHandles.size() > 0) {
            chatId = inspectedHandles.at(0);
            attributes[TP_QT_IFACE_CONNECTION+"/contact-id"] = chatId;
        } else {
            continue;
        }
        ClientEntity entity = mEntities[chatId];
        if (ifaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE)) {
            Tp::SimplePresence presence;
            presence.status = "unknown";
            presence.type = Tp::ConnectionPresenceTypeUnknown;
            if (entity.has_presence()) {
                ClientPresence clientPresence = entity.presence();
                if (handle == selfHandle()) {
                    presence.status = clientPresence.available() ? "available" : "hidden";
                    presence.type = clientPresence.available() ? Tp::ConnectionPresenceTypeAvailable : Tp::ConnectionPresenceTypeHidden;
                } else {
                    presence.status = clientPresence.available() ? "available" : "offline";
                    presence.type = clientPresence.available() ? Tp::ConnectionPresenceTypeAvailable : Tp::ConnectionPresenceTypeOffline;
                }
            }
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_SIMPLE_PRESENCE+"/presence"] = QVariant::fromValue(presence);
        }

        if (ifaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING)) {
            QString alias = "Unknown";
            if (entity.has_properties() && entity.properties().has_displayname()) {
                alias = entity.properties().displayname().c_str();
            } else if (mOtherContacts.contains(chatId)) {
                alias = mOtherContacts[chatId].fallbackname().c_str();
            }
            attributes[TP_QT_IFACE_CONNECTION_INTERFACE_ALIASING + QLatin1String("/alias")] = QVariant::fromValue(alias);
        }
        if (ifaces.contains(TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS)) {
            if (entity.has_properties() && entity.properties().has_photourl()) {
                QString url = QString("https:") + QString(entity.properties().photourl().c_str());
                QString token = url.toLatin1().toBase64();
                attributes[TP_QT_IFACE_CONNECTION_INTERFACE_AVATARS + QLatin1String("/token")] = QVariant::fromValue(token);
            }
        }
        attributesMap[handle] = attributes;
    }

    return attributesMap;
}

QStringList RingConnection::inspectHandles(uint handleType, const Tp::UIntList& handles, Tp::DBusError *error)
{
    QStringList identifiers;

    if ( handleType != Tp::HandleTypeContact && handleType != Tp::HandleTypeRoom) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT,"Not supported");
        return QStringList();
    }
    QMap<uint, QString> handleMap = handleType == Tp::HandleTypeContact ? mContactHandles : mRoomHandles;
    Q_FOREACH( uint handle, handles) {
        if (handleMap.keys().contains(handle)) {
            identifiers.append(handleMap.value(handle));
        } else {
            qDebug() << "Not found " << handle;
            error->set(TP_QT_ERROR_INVALID_HANDLE, "Handle not found");
            return QStringList();
        }
    }
    qDebug() << "RingConnection::inspectHandles " << identifiers;
    return identifiers;
}

Tp::UIntList RingConnection::requestHandles(uint handleType, const QStringList& identifiers, Tp::DBusError* error)
{
    Tp::UIntList handles;

    if ( handleType != Tp::HandleTypeContact && handleType != Tp::HandleTypeRoom) {
        error->set(TP_QT_ERROR_INVALID_ARGUMENT, "Not supported");
        return Tp::UIntList();
    }

    QMap<uint, QString> handleMap = handleType == Tp::HandleTypeContact ? mContactHandles : mRoomHandles;
    Q_FOREACH( const QString& identifier, identifiers) {
        if (handleMap.values().contains(identifier)) {
            handles.append(handleMap.key(identifier));
        } else {
            if (handleType == Tp::HandleTypeContact) {
                handles.append(newContactHandle(identifier));
            } else {
                handles.append(newRoomHandle(identifier));
            }
        }
    }

    return handles;
}

QString RingConnection::conversationIdForChatId(const QString &chatId)
{
    QMap<QString, ClientConversationState>::iterator i = mConversations.begin();
    for (i = mConversations.begin(); i != mConversations.end(); ++i) {
        if (i->conversation().type() != STICKY_ONE_TO_ONE) {
            continue;
        }
        for (int j = 0; j < i->conversation().currentparticipant_size(); j++) {
            if (i->conversation().currentparticipant(j).chatid().c_str() == chatId) {
                return i->conversation().id().id().c_str();
            }
        }
    }
    return QString();
}

QMap<QString, ClientConversationState> RingConnection::getConversations()
{
    return mConversations;
}

Tp::BaseChannelPtr RingConnection::createRoomListChannel()
{
    Tp::BaseChannelPtr baseChannel = Tp::BaseChannel::create(this, TP_QT_IFACE_CHANNEL_TYPE_ROOM_LIST);

    roomListChannel = Tp::BaseChannelRoomListType::create();
    roomListChannel->setListRoomsCallback(Tp::memFun(this, &RingConnection::roomListStartListing));
    roomListChannel->setStopListingCallback(Tp::memFun(this, &RingConnection::roomListStopListing));
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(roomListChannel));

    return baseChannel;
}

void RingConnection::roomListStartListing(Tp::DBusError *error)
{
    Q_UNUSED(error)

    QTimer::singleShot(0, this, SLOT(onPopulateRoomList()));
    roomListChannel->setListingRooms(true);
}

void RingConnection::roomListStopListing(Tp::DBusError *error)
{
    Q_UNUSED(error)
    roomListChannel->setListingRooms(false);
}

void RingConnection::onPopulateRoomList()
{
    Tp::RoomInfoList rooms;

    Q_FOREACH (const QString &conversationId, mConversations.keys()) {
        ClientConversationState conversation = mConversations[conversationId];
        QString conversationName(conversation.conversation().name().c_str());
        Tp::RoomInfo roomInfo;

        if (conversation.conversation().type() != GROUP) {
            continue;
        }

        if (conversationName.isEmpty()) {
            QStringList participants;
            for (int i = 0; i< conversation.conversation().participantdata_size(); i++) {
                QString chatId(conversation.conversation().participantdata(i).id().chatid().c_str());
                QString fallbackName(conversation.conversation().participantdata(i).fallbackname().c_str());
                if (mEntities.contains(chatId)) {
                    participants << mEntities[chatId].properties().displayname().c_str();
                } else if (!fallbackName.isEmpty()) {
                    participants << fallbackName;
                }
            }
            conversationName = participants.join(", ");
        }

        roomInfo.channelType = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
        roomInfo.handle = ensureRoomHandle(conversationId);
        roomInfo.info[QLatin1String("handle-name")] = conversationId;
        roomInfo.info[QLatin1String("members-only")] = true;
        roomInfo.info[QLatin1String("invite-only")] = true;
        roomInfo.info[QLatin1String("password")] = false;
        roomInfo.info[QLatin1String("name")] = conversationName;
        roomInfo.info[QLatin1String("members")] = conversation.conversation().currentparticipant_size();
        rooms << roomInfo;
    }

    roomListChannel->gotRooms(rooms);
    roomListChannel->setListingRooms(false);
}

Tp::BaseChannelPtr RingConnection::createChannel(const QVariantMap &request, Tp::DBusError *error)
{
    const QString channelType = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")).toString();
    uint targetHandleType = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")).toUInt();
    uint targetHandle = 0;
    uint initiatorHandle = 0;
    QString conversationId;
    QString targetID;
    QStringList participants;
    HangingTextChannel *channel = NULL;

    if (channelType == TP_QT_IFACE_CHANNEL_TYPE_ROOM_LIST) {
        return createRoomListChannel();
    }

    switch (targetHandleType) {
    case Tp::HandleTypeContact:
        if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
            targetHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
            targetID = mContactHandles.value(targetHandle);
        } else if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
            targetID = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString();
            targetHandle = ensureContactHandle(targetID);
        }
        break;
    case Tp::HandleTypeRoom:
        if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle"))) {
            targetHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")).toUInt();
            targetID = mRoomHandles.value(targetHandle);
        } else if (request.contains(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID"))) {
            targetID = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".TargetID")).toString();
            targetHandle = ensureRoomHandle(targetID);
        }
    default:
        break;
    }

    if (request.contains("conversationId")) {
        conversationId = request["conversationId"].toString();
    } else {
        if (targetHandleType == Tp::HandleTypeRoom) {
            QStringList inspectedHandles = inspectHandles(Tp::HandleTypeRoom, Tp::UIntList() << targetHandle, error);
            if (inspectedHandles.size() == 1) {
                conversationId = inspectedHandles.at(0);
            }
        } else {
            conversationId = conversationIdForChatId(targetID);
        }
    }

    if (mConversations.contains(conversationId)) {
        ClientConversationState conversation = mConversations[conversationId];
        for (int i = 0; i < conversation.conversation().currentparticipant_size(); i++) {
            QString chatId = conversation.conversation().currentparticipant(i).chatid().c_str();
            if (ensureContactHandle(chatId) == selfHandle()) {
                continue;
            }
            participants << chatId;
        }
    }

    if (targetHandleType == Tp::HandleTypeContact) {
        initiatorHandle = request.value(TP_QT_IFACE_CHANNEL + QLatin1String(".InitiatorHandle"), selfHandle()).toUInt();
    }

    if (targetHandleType == Tp::HandleTypeRoom) {
        participants << inspectHandles(Tp::HandleTypeContact, qdbus_cast<Tp::UIntList>(request[TP_QT_IFACE_CHANNEL_INTERFACE_CONFERENCE + QLatin1String(".InitialInviteeHandles")]), error);
        participants << selfID();
        participants.removeDuplicates();
        channel = new HangingTextChannel(this, conversationId, QString(), participants);
    } else {
        participants << mContactHandles.value(targetHandle);
        channel = new HangingTextChannel(this, conversationId, participants.at(0), participants);
    }
    channel->baseChannel()->setInitiatorHandle(initiatorHandle);

    mTextChannels << channel;
    QObject::connect(channel, SIGNAL(destroyed()), SLOT(onTextChannelClosed()));
    return channel->baseChannel();
}

void RingConnection::onClientSyncAllNewEventsResponse(ClientSyncAllNewEventsResponse &csaner)
{
    QStringList conversationIds;

    conversationIds = mSettings->value("conversationIds", QStringList()).toString().split(",");
    for (int i=0; i<csaner.conversationstate_size(); i++) {
        QString conversationId = csaner.conversationstate(i).conversationid().id().c_str();
        mConversations[conversationId] = csaner.conversationstate(i);
        conversationIds << conversationId;
        // TODO: retrieve new events and process them as scrollback messages
        for (int j = 0; j < csaner.conversationstate(i).event_size(); j++) {
            mPendingNewEvents << csaner.conversationstate(i).event(j);
        }
    }
    conversationIds.removeDuplicates();
    conversationIds.removeAll("");
    mSettings->setValue("conversationIds", conversationIds.join(","));

    // probably a new account with no conversations yet
    if (conversationIds.isEmpty()) {
        setStatus(Tp::ConnectionStatusConnected, Tp::ConnectionStatusReasonRequested);
        if (!mEntities.isEmpty()) {
            mHangishClient->queryPresence(mEntities.keys());
        }
        return;
    }

    Q_FOREACH(const QString &conversationId, conversationIds) {
        mPendingRequests << getConversation(conversationId);
    }
}

void RingConnection::onClientQueryPresenceResponse(quint64,ClientQueryPresenceResponse &cqprp)
{
    Tp::SimpleContactPresences presences;

    for (int i = 0; i < cqprp.presenceresult_size(); i++) {
        ClientPresence presence = cqprp.presenceresult(i).presence();
        QString chatId = cqprp.presenceresult(i).userid().chatid().c_str();
        uint handle = ensureContactHandle(chatId);

        // skip our own presence if present
        if (handle == selfHandle()) {
            continue;
        }

        // replace current presence of this entity
        mEntities[chatId].set_allocated_presence(new ClientPresence(presence));

        // inform telepathy clients about the new online status,
        // on hangouts we have only available and not available statuses
        Tp::SimplePresence tpPresence;
        if (presence.available()) {
            tpPresence.status = "available";
            tpPresence.type = Tp::ConnectionPresenceTypeAvailable;
        } else {
            tpPresence.status = "offline";
            tpPresence.type = Tp::ConnectionPresenceTypeOffline;
        }
        presences[ensureContactHandle(chatId)] = tpPresence;
    }

    simplePresenceIface->setPresences(presences);
}

void RingConnection::onClientGetConversationResponse(quint64 requestId, ClientGetConversationResponse &cgcr)
{
    QString conversationId = cgcr.conversationstate().conversationid().id().c_str();
    mConversations[conversationId] = cgcr.conversationstate();

    for (int i = 0; i < cgcr.conversationstate().conversation().participantdata_size(); i++) {
        ClientConversationParticipantData participantData = cgcr.conversationstate().conversation().participantdata(i);
        // if this participant is not known, save it so we can at least use its fallback name later
        if (!mEntities.contains(participantData.id().chatid().c_str())) {
            mOtherContacts[participantData.id().chatid().c_str()] = participantData;
        }
    }

    mPendingRequests.removeOne(requestId);

    // if we got information about all chats, just signalize we are finally connected
    if (mPendingRequests.isEmpty() && status() == Tp::ConnectionStatusConnecting) {
        setStatus(Tp::ConnectionStatusConnected, Tp::ConnectionStatusReasonRequested);
        if (!mEntities.isEmpty()) {
            mHangishClient->queryPresence(mEntities.keys());
        }
        // process any new event we received while we were offline
        Q_FOREACH (ClientEvent event, mPendingNewEvents) {
            processClientEvent(event, true);
        }
        mPendingNewEvents.clear();
    }

    if (mPendingEventsWaitingForConversation.contains(conversationId)) {
        Q_FOREACH(ClientEvent event, mPendingEventsWaitingForConversation[conversationId]) {
            processClientEvent(event, true);
        }
        mPendingEventsWaitingForConversation.remove(conversationId);
    }
}

HangingTextChannel* RingConnection::ensureTextChannel(const QString &conversationId, const QString &fromId)
{
    QVariantMap request;
    ClientConversationState conversation = mConversations[conversationId];
    HangingTextChannel *channel = textChannelForConversationId(conversationId);

    if (channel) {
        return channel;
    }

    Tp::UIntList list;
    for (int i=0; i< conversation.conversation().currentparticipant_size(); i++) {
        uint handle = ensureContactHandle(conversation.conversation().currentparticipant(i).chatid().c_str());
        if (handle != selfHandle()) {
            list << handle;
        }
    }

    Tp::DBusError error;
    bool yours;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".ChannelType")] = TP_QT_IFACE_CHANNEL_TYPE_TEXT;
    request[TP_QT_IFACE_CHANNEL + QLatin1String(".InitiatorHandle")] = ensureContactHandle(fromId);

    if (conversation.conversation().type() == STICKY_ONE_TO_ONE) {
        request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")] = Tp::HandleTypeContact;
        request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")] = ensureContactHandle(fromId);
        request["conversationId"] = conversationId;
    } else {
        request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandleType")] = Tp::HandleTypeRoom;
        request[TP_QT_IFACE_CHANNEL + QLatin1String(".TargetHandle")] = ensureRoomHandle(conversationId);
        request[TP_QT_IFACE_CHANNEL_INTERFACE_CONFERENCE + QLatin1String(".InitialInviteeHandles")] = QVariant::fromValue(list);
    }
    ensureChannel(request, yours, false, &error);

    if (error.isValid()) {
        qWarning() << "Error creating channel for incoming message" << error.name() << error.message();
        return NULL;
    }

    return textChannelForConversationId(conversationId);
}

void RingConnection::processClientEvent(ClientEvent &event, bool scrollback)
{
    QString conversationId(event.conversationid().id().c_str());
    if (!mConversations.contains(conversationId)) {
        bool shouldRetrieve = !mPendingEventsWaitingForConversation.contains(conversationId);
        mPendingEventsWaitingForConversation[conversationId] << event;
        if (shouldRetrieve) {
            getConversation(conversationId);
        }
        return;
    }

    QString fromId(event.senderid().chatid().c_str());
    HangingTextChannel *channel = ensureTextChannel(conversationId, fromId);
    if (channel) {
        channel->eventReceived(event, scrollback);
        return;
    }
}

void RingConnection::onClientStateUpdate(ClientStateUpdate &update)
{
    if (update.has_stateupdateheader() && update.stateupdateheader().has_currentservertime()) {
        mLastKnownUpdate = update.stateupdateheader().currentservertime();
    }

    if (update.has_presencenotification()) {
        for (int i=0; i<update.presencenotification().presence_size(); i++) {
            ClientPresenceResult presence = update.presencenotification().presence(i);
            QString chatId = presence.userid().chatid().c_str();
            uint handle = ensureContactHandle(chatId);
            if (handle == selfHandle() || !mEntities.contains(chatId)) {
                continue;
            }
            mEntities[chatId].set_allocated_presence(new ClientPresence(presence.presence()));
            Tp::SimpleContactPresences presences;
            Tp::SimplePresence tpPresence;
            if (presence.presence().available()) {
                tpPresence.status = "available";
                tpPresence.type = Tp::ConnectionPresenceTypeAvailable;
            } else {
                tpPresence.status = "offline";
                tpPresence.type = Tp::ConnectionPresenceTypeOffline;
            }
            presences[ensureContactHandle(presence.userid().chatid().c_str())] = tpPresence;
            simplePresenceIface->setPresences(presences);
        }
    }

    if (update.has_eventnotification()) {
        ClientEventNotification notification = update.eventnotification();
        if (notification.has_event()) {
            ClientEvent event = notification.event();
            processClientEvent(event);
        }
    }

    if (update.has_typingnotification()) {
        // only notify if the channel exists
        HangingTextChannel *channel = textChannelForConversationId(update.typingnotification().conversationid().id().c_str());
        if (channel) {
            channel->updateTypingState(update.typingnotification());
        }
    }
}

void RingConnection::onTextChannelClosed()
{
    HangingTextChannel *channel = static_cast<HangingTextChannel*>(sender());
    if (channel) {
        qDebug() << "text channel closed";
        mTextChannels.removeAll(channel);
    }
}

uint RingConnection::ensureRoomHandle(const QString &id)
{
    Q_FOREACH(const QString &thisId, mRoomHandles.values()) {
        if (id == thisId) {
            // this Room already exists
            return mRoomHandles.key(id);
        }
    }
    qDebug() << "RingConnection::ensureRoomHandle" << id;
    return newRoomHandle(id);
}

uint RingConnection::ensureContactHandle(const QString &id)
{
    Q_FOREACH(const QString &thisId, mContactHandles.values()) {
        if (id == thisId) {
            // this user already exists
            return mContactHandles.key(id);
        }
    }
    qDebug() << "RingConnection::ensureContactHandle" << id;
    return newContactHandle(id);
}

uint RingConnection::newContactHandle(const QString &identifier)
{
    mContactHandles[++mHandleCount] = identifier;
    return mHandleCount;
}

uint RingConnection::newRoomHandle(const QString &identifier)
{
    mRoomHandles[++mHandleCount] = identifier;
    return mHandleCount;
}

quint64 RingConnection::getConversation(const QString &conversationId, bool includeEvent)
{
    ClientGetConversationRequest clientGetConversationRequest;
    ClientConversationSpec *clientConversationSpec =  new ClientConversationSpec();
    ClientConversationId *clientConversationId = new ClientConversationId();

    clientGetConversationRequest.set_includeevent(includeEvent);
    clientConversationSpec->set_allocated_conversationid(clientConversationId);
    clientConversationId->set_id(conversationId.toLatin1().data());

    clientGetConversationRequest.set_allocated_conversationspec(clientConversationSpec);
    return mHangishClient->getConversation(clientGetConversationRequest);
}

QString RingConnection::uniqueName() const
{
    QString timestamp(QString::number(QDateTime::currentMSecsSinceEpoch()));
    QString md5(QCryptographicHash::hash(timestamp.toLatin1(), QCryptographicHash::Md5).toHex());
    return QString(QLatin1String("connection_%1")).arg(md5);
}
