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

 #ifndef RING_PROTOCOL_CONNECTION_H
 #define RING_PROTOCOL_CONNECTION_H

 // telepathy-qt
 #include <TelepathyQt/BaseConnection>
 #include <TelepathyQt/BaseChannel>
 #include <TelepathyQt/AbstractAdaptor>
 #include <TelepathyQt/DBusError>

 #include "ringtextchannel.h"

class RingTextChannel;

class RingConnection : public Tp::BaseConnection
{
    Q_OBJECT
    Q_DISABLE_COPY(RingConnection)
public:
    HangingConnection(const QDBusConnection &dbusConnection,
                      const QString &cmName,
                      const QString &protocolName,
                      const QVariantMap &parameters);

    // telepathy callbacks

    // Tp::BaseConnection
    QStringList inspectHandles(uint handleType, const Tp::UIntList& handles, Tp::DBusError *error);
    Tp::UIntList requestHandles(uint handleType, const QStringList& identifiers, Tp::DBusError* error);
    Tp::BaseChannelPtr createChannel(const QVariantMap &request, Tp::DBusError *error);
    void connect(Tp::DBusError *error);

    // Tp::BaseConnectionContactsInterface
    Tp::ContactAttributesMap getContactAttributes(const Tp::UIntList &handles, const QStringList &ifaces, Tp::DBusError *error);

    // Tp::BaseConnectionContactListInterface
    Tp::ContactAttributesMap getContactListAttributes(const QStringList &interfaces, bool hold, Tp::DBusError *error);

    // Tp::BaseConnectionAvatarsInterface
    Tp::AvatarTokenMap getKnownAvatarTokens(const Tp::UIntList &contacts, Tp::DBusError *error);
    void requestAvatars(const Tp::UIntList &contacts, Tp::DBusError *error);

    // Tp::BaseConnectionSimplePresenceInterface
    uint setPresence(const QString& newStatus, const QString& statusMessage, Tp::DBusError *error);

    // Tp::BaseChannelRoomListType
    void roomListStartListing(Tp::DBusError *error);
    void roomListStopListing(Tp::DBusError *error);

    uint ensureContactHandle(const QString &chatId);
    uint ensureRoomHandle(const QString &id);
    HangingTextChannel* textChannelForMembers(const QStringList &members);
    HangingTextChannel* textChannelForConversationId(const QString &conversationId);
    QString conversationIdForChatId(const QString &chatId);
    HangishClient *hangishClient();
    QMap<QString, ClientConversationState> getConversations();
    QString uniqueName() const;

    ~RingConnection();

protected:
    Tp::BaseChannelPtr createRoomListChannel();

private Q_SLOTS:
    void onClientStateUpdate(ClientStateUpdate &update);
    void onClientGetConversationResponse(quint64 requestId, ClientGetConversationResponse &cgcr);
    void onClientSyncAllNewEventsResponse(ClientSyncAllNewEventsResponse &csaner);
    void onClientSetPresenceResponse(quint64 requestId, ClientSetPresenceResponse &cspr);
    void onClientQueryPresenceResponse(quint64,ClientQueryPresenceResponse &cqprp);

    void onTextChannelClosed();
    void onInitFinished();
    void onDisconnected();
    void onAuthFailed(AuthenticationStatus status, QString reason);
    void on2FactorAuthRequired();
    void onLoginNeeded();
    void onAvatarDownloaded();
    void onPopulateRoomList();
    void onConnectionStatusChanged(ConnectionStatus status);

private:
    uint newContactHandle(const QString &identifier);
    uint newRoomHandle(const QString &identifier);
    void startMechanismWithData(const QString &mechanism, const QByteArray &data, Tp::DBusError *error);
    void processClientEvent(ClientEvent &event, bool scrollback = false);
    HangingTextChannel* ensureTextChannel(const QString &conversationId, const QString &fromId);
    quint64 getConversation(const QString &conversationId, bool includeEvent = false);

    QMap<uint, QString> mContactHandles;
    QMap<uint, QString> mRoomHandles;

    QList<HangingTextChannel*> mTextChannels;
    quint64 mLastKnownUpdate;
    uint mHandleCount;
    Tp::SimplePresence mSelfPresence;
    QString mTokenPath;
    QString mAccount;
    QString mPassword;
    QString mConfigurationDirectory;
    HangishClient *mHangishClient;
    QString mRequestedPresence;
    QSettings *mSettings;
    QMap<QString, ClientConversationState> mConversations;
    QMap<QString, ClientEntity> mEntities;
    QMap<QString, QString> mPendingAvatars;
    QNetworkAccessManager mNetworkManager;
    QList<quint64> mPendingRequests;
    QMap<QString, ClientConversationParticipantData> mOtherContacts;
    QList<ClientEvent> mPendingNewEvents;
    QMap<QString, QList<ClientEvent> > mPendingEventsWaitingForConversation;
    Tp::ConnectionStatusReason mDisconnectReason;

    Tp::BaseChannelSASLAuthenticationInterfacePtr saslIface;
    Tp::BaseConnectionContactListInterfacePtr contactListIface;
    Tp::BaseConnectionAvatarsInterfacePtr avatarsIface;
    Tp::BaseConnectionRequestsInterfacePtr requestsIface;
    Tp::BaseConnectionSimplePresenceInterfacePtr simplePresenceIface;
    Tp::BaseConnectionContactsInterfacePtr contactsIface;
    Tp::BaseChannelRoomListTypePtr roomListChannel;
};

#endif
