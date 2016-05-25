#ifndef RING_TEXT_CHANNEL_H
#define RING_TEXT_CHANNEL_H

#include <QObject>

#include <TelepathyQt/Constants>
#include <TelepathyQt/BaseChannel>
#include <TelepathyQt/Types>
#include <TelepathyQt/DBusError>
#include <QString>

#include "Connection.h"

class HangingConnection;

class RingTextChannel : public QObject
{
    Q_OBJECT
public:
    RingTextChannel(HangingConnection *conn, const QString &conversationId, const QString userChatId, QStringList phoneNumbers, QObject *parent = 0);

    //telepathy callbacks
    QString sendMessage(Tp::MessagePartList message, uint flags, Tp::DBusError* error);
    void onAddMembers(const Tp::UIntList& handles, const QString& message, Tp::DBusError* error);
    void onRemoveMembers(const Tp::UIntList& handles, const QString& message, uint reason, Tp::DBusError* error);
    void setChatState(uint state, Tp::DBusError *error);

    void eventReceived(ClientEvent &event, bool scrollback = false);
    void updateTypingState(const ClientSetTypingNotification &notification);
    Tp::BaseChannelPtr baseChannel();
    void sendDeliveryReport(const QString &messageId, QString identifier, Tp::DeliveryStatus status);
    QString conversationId();

private Q_SLOTS:
    void onTypingTimeout();

private:
    ~RingTextChannel();
    Tp::BaseChannelPtr mBaseChannel;
    HangingConnection *mConnection;
    uint mMessageCounter;
    QString mConversationId;
    QString mUserChatId;
    QMap<uint, QString> mPendingMessages;
    QTimer mTypingTimer;

    Tp::BaseChannelMessagesInterfacePtr mMessagesIface;
    Tp::BaseChannelGroupInterfacePtr mGroupIface;
    Tp::BaseChannelTextTypePtr mTextChannel;
    Tp::BaseChannelRoomInterfacePtr mRoomIface;
    Tp::BaseChannelRoomConfigInterfacePtr mRoomConfigIface;
    Tp::BaseChannelChatStateInterfacePtr mChatStateIface;
};

#endif // RingTextChannel_H
