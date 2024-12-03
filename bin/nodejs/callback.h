#pragma once

#include <uv.h>

#include <queue>
#include <functional>
#include <mutex>
#include <string_view>

using namespace v8;

Persistent<Function> accountsChangedCb;
Persistent<Function> accountDetailsChangedCb;
Persistent<Function> registrationStateChangedCb;
Persistent<Function> composingStatusChangedCb;
Persistent<Function> volatileDetailsChangedCb;
Persistent<Function> incomingAccountMessageCb;
Persistent<Function> accountMessageStatusChangedCb;
Persistent<Function> needsHostCb;
Persistent<Function> activeCallsChangedCb;
Persistent<Function> incomingTrustRequestCb;
Persistent<Function> contactAddedCb;
Persistent<Function> contactRemovedCb;
Persistent<Function> nameRegistrationEndedCb;
Persistent<Function> knownDevicesChangedCb;
Persistent<Function> registeredNameFoundCb;
Persistent<Function> callStateChangedCb;
Persistent<Function> mediaChangeRequestedCb;
Persistent<Function> incomingMessageCb;
Persistent<Function> incomingCallCb;
Persistent<Function> incomingCallWithMediaCb;
Persistent<Function> dataTransferEventCb;
Persistent<Function> conversationLoadedCb;
Persistent<Function> swarmLoadedCb;
Persistent<Function> messagesFoundCb;
Persistent<Function> messageReceivedCb;
Persistent<Function> swarmMessageReceivedCb;
Persistent<Function> swarmMessageUpdatedCb;
Persistent<Function> reactionAddedCb;
Persistent<Function> reactionRemovedCb;
Persistent<Function> conversationProfileUpdatedCb;
Persistent<Function> conversationRequestReceivedCb;
Persistent<Function> conversationRequestDeclinedCb;
Persistent<Function> conversationReadyCb;
Persistent<Function> conversationRemovedCb;
Persistent<Function> conversationMemberEventCb;
Persistent<Function> onConversationErrorCb;
Persistent<Function> conferenceCreatedCb;
Persistent<Function> conferenceChangedCb;
Persistent<Function> conferenceRemovedCb;
Persistent<Function> onConferenceInfosUpdatedCb;
Persistent<Function> conversationPreferencesUpdatedCb;
Persistent<Function> messageSendCb;
Persistent<Function> accountProfileReceivedCb;
Persistent<Function> profileReceivedCb;
Persistent<Function> userSearchEndedCb;
Persistent<Function> deviceRevocationEndedCb;
Persistent<Function> subscriptionStateChangedCb;
Persistent<Function> nearbyPeerNotificationCb;
Persistent<Function> newBuddyNotificationCb;
Persistent<Function> serverErrorCb;
Persistent<Function> newServerSubscriptionRequestCb;
Persistent<Function> deviceAuthStateChangedCb;
Persistent<Function> addDeviceStateChangedCb;

std::queue<std::function<void()>> pendingSignals;
std::mutex pendingSignalsLock;

uv_async_t signalAsync;

Persistent<Function>*
getPresistentCb(std::string_view signal)
{
    if (signal == "AccountsChanged")
        return &accountsChangedCb;
    else if (signal == "AccountDetailsChanged")
        return &accountDetailsChangedCb;
    else if (signal == "RegistrationStateChanged")
        return &registrationStateChangedCb;
    else if (signal == "ComposingStatusChanged")
        return &composingStatusChangedCb;
    else if (signal == "VolatileDetailsChanged")
        return &volatileDetailsChangedCb;
    else if (signal == "IncomingAccountMessage")
        return &incomingAccountMessageCb;
    else if (signal == "AccountMessageStatusChanged")
        return &accountMessageStatusChangedCb;
    else if (signal == "NeedsHost")
        return &needsHostCb;
    else if (signal == "ActiveCallsChanged")
        return &activeCallsChangedCb;
    else if (signal == "IncomingTrustRequest")
        return &incomingTrustRequestCb;
    else if (signal == "ContactAdded")
        return &contactAddedCb;
    else if (signal == "ContactRemoved")
        return &contactRemovedCb;
    else if (signal == "NameRegistrationEnded")
        return &nameRegistrationEndedCb;
    else if (signal == "KnownDevicesChanged")
        return &knownDevicesChangedCb;
    else if (signal == "RegisteredNameFound")
        return &registeredNameFoundCb;
    else if (signal == "CallStateChanged")
        return &callStateChangedCb;
    else if (signal == "MediaChangeRequested")
        return &mediaChangeRequestedCb;
    else if (signal == "IncomingMessage")
        return &incomingMessageCb;
    else if (signal == "IncomingCall")
        return &incomingCallCb;
    else if (signal == "IncomingCallWithMedia")
        return &incomingCallWithMediaCb;
    else if (signal == "DataTransferEvent")
        return &dataTransferEventCb;
    else if (signal == "ConversationLoaded")
        return &conversationLoadedCb;
    else if (signal == "SwarmLoaded")
        return &swarmLoadedCb;
    else if (signal == "MessagesFound")
        return &messagesFoundCb;
    else if (signal == "MessageReceived")
        return &messageReceivedCb;
    else if (signal == "SwarmMessageReceived")
        return &swarmMessageReceivedCb;
    else if (signal == "SwarmMessageUpdated")
        return &swarmMessageUpdatedCb;
    else if (signal == "ReactionAdded")
        return &reactionAddedCb;
    else if (signal == "ReactionRemoved")
        return &reactionRemovedCb;
    else if (signal == "ConversationProfileUpdated")
        return &conversationProfileUpdatedCb;
    else if (signal == "ConversationReady")
        return &conversationReadyCb;
    else if (signal == "ConversationRemoved")
        return &conversationRemovedCb;
    else if (signal == "ConversationRequestReceived")
        return &conversationRequestReceivedCb;
    else if (signal == "ConversationRequestDeclined")
        return &conversationRequestDeclinedCb;
    else if (signal == "ConversationMemberEvent")
        return &conversationMemberEventCb;
    else if (signal == "OnConversationError")
        return &onConversationErrorCb;
    else if (signal == "ConferenceCreated")
        return &conferenceCreatedCb;
    else if (signal == "ConferenceChanged")
        return &conferenceChangedCb;
    else if (signal == "ConferenceRemoved")
        return &conferenceRemovedCb;
    else if (signal == "OnConferenceInfosUpdated")
        return &onConferenceInfosUpdatedCb;
    else if (signal == "ConversationPreferencesUpdated")
        return &conversationPreferencesUpdatedCb;
    else if (signal == "LogMessage")
        return &messageSendCb;
    else if (signal == "AccountProfileReceived")
        return &accountProfileReceivedCb;
    else if (signal == "ProfileReceived")
        return &profileReceivedCb;
    else if (signal == "UserSearchEnded")
        return &userSearchEndedCb;
    else if (signal == "DeviceRevocationEnded")
        return &deviceRevocationEndedCb;
    else if (signal == "SubscriptionStateChanged")
        return &subscriptionStateChangedCb;
    else if (signal == "NearbyPeerNotification")
        return &nearbyPeerNotificationCb;
    else if (signal == "NewBuddyNotification")
        return &newBuddyNotificationCb;
    else if (signal == "ServerError")
        return &serverErrorCb;
    else if (signal == "NewServerSubscriptionRequest")
        return &newServerSubscriptionRequestCb;
    else if (signal == "DeviceAuthStateChanged")
        return &deviceAuthStateChangedCb;
    else if (signal == "AddDeviceStateChanged")
        return &addDeviceStateChangedCb;
    else
        return nullptr;
}

#define V8_STRING_LITERAL(str) v8::String::NewFromUtf8Literal(v8::Isolate::GetCurrent(), str)

#define V8_STRING_NEW(str) \
    v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), \
                            str.data(), \
                            v8::NewStringType::kNormal, \
                            str.size())
#define V8_STRING_NEW_LOCAL(str) V8_STRING_NEW(str).ToLocalChecked()

inline std::string_view
toView(const String::Utf8Value& utf8)
{
    return {*utf8, (size_t) utf8.length()};
}

inline SWIGV8_ARRAY
intVectToJsArray(const std::vector<uint8_t>& intVect)
{
    SWIGV8_ARRAY jsArray = SWIGV8_ARRAY_NEW(intVect.size());
    for (unsigned int i = 0; i < intVect.size(); i++)
        jsArray->Set(SWIGV8_CURRENT_CONTEXT(),
                     SWIGV8_INTEGER_NEW_UNS(i),
                     SWIGV8_INTEGER_NEW(intVect[i]));
    return jsArray;
}

inline SWIGV8_OBJECT
stringMapToJsMap(const std::map<std::string, std::string>& strmap)
{
    SWIGV8_OBJECT jsMap = SWIGV8_OBJECT_NEW();
    for (auto& kvpair : strmap)
        jsMap->Set(SWIGV8_CURRENT_CONTEXT(),
                   V8_STRING_NEW_LOCAL(std::get<0>(kvpair)),
                   V8_STRING_NEW_LOCAL(std::get<1>(kvpair)));
    return jsMap;
}

inline SWIGV8_OBJECT
stringIntMapToJsMap(const std::map<std::string, int32_t>& strmap)
{
    SWIGV8_OBJECT jsMap = SWIGV8_OBJECT_NEW();
    for (auto& kvpair : strmap)
        jsMap->Set(SWIGV8_CURRENT_CONTEXT(),
                   V8_STRING_NEW_LOCAL(std::get<0>(kvpair)),
                   SWIGV8_INTEGER_NEW(std::get<1>(kvpair)));
    return jsMap;
}

inline SWIGV8_ARRAY
stringMapVecToJsMapArray(const std::vector<std::map<std::string, std::string>>& vect)
{
    SWIGV8_ARRAY jsArray = SWIGV8_ARRAY_NEW(vect.size());
    for (unsigned int i = 0; i < vect.size(); i++)
        jsArray->Set(SWIGV8_CURRENT_CONTEXT(), SWIGV8_INTEGER_NEW_UNS(i), stringMapToJsMap(vect[i]));
    return jsArray;
}

inline SWIGV8_OBJECT
swarmMessageToJs(const libjami::SwarmMessage& message)
{
    SWIGV8_OBJECT jsMap = SWIGV8_OBJECT_NEW();
    jsMap->Set(SWIGV8_CURRENT_CONTEXT(), V8_STRING_LITERAL("id"), V8_STRING_NEW_LOCAL(message.id));
    jsMap->Set(SWIGV8_CURRENT_CONTEXT(),
               V8_STRING_LITERAL("type"),
               V8_STRING_NEW_LOCAL(message.type));
    jsMap->Set(SWIGV8_CURRENT_CONTEXT(),
               V8_STRING_LITERAL("linearizedParent"),
               V8_STRING_NEW_LOCAL(message.linearizedParent));
    jsMap->Set(SWIGV8_CURRENT_CONTEXT(), V8_STRING_LITERAL("body"), stringMapToJsMap(message.body));
    jsMap->Set(SWIGV8_CURRENT_CONTEXT(),
               V8_STRING_LITERAL("reactions"),
               stringMapVecToJsMapArray(message.reactions));
    jsMap->Set(SWIGV8_CURRENT_CONTEXT(),
               V8_STRING_LITERAL("editions"),
               stringMapVecToJsMapArray(message.editions));
    jsMap->Set(SWIGV8_CURRENT_CONTEXT(),
               V8_STRING_LITERAL("status"),
               stringIntMapToJsMap(message.status));
    return jsMap;
}

inline SWIGV8_ARRAY
swarmMessagesToJsArray(const std::vector<libjami::SwarmMessage>& messages)
{
    SWIGV8_ARRAY jsArray = SWIGV8_ARRAY_NEW(messages.size());
    for (unsigned int i = 0; i < messages.size(); i++)
        jsArray->Set(SWIGV8_CURRENT_CONTEXT(),
                     SWIGV8_INTEGER_NEW_UNS(i),
                     swarmMessageToJs(messages[i]));
    return jsArray;
}

void
setCallback(std::string_view signal, Local<Function>& func)
{
    if (auto* presistentCb = getPresistentCb(signal)) {
        if (func->IsObject() && func->IsFunction()) {
            presistentCb->Reset(Isolate::GetCurrent(), func);
        } else {
            presistentCb->Reset();
        }
    } else {
        printf("No Signal Associated with Event \'%.*s\'\n", (int) signal.size(), signal.data());
    }
}

void
parseCbMap(const SWIGV8_VALUE& callbackMap)
{
    SWIGV8_OBJECT cbAssocArray = callbackMap->ToObject(SWIGV8_CURRENT_CONTEXT()).ToLocalChecked();
    SWIGV8_ARRAY props = cbAssocArray->GetOwnPropertyNames(SWIGV8_CURRENT_CONTEXT()).ToLocalChecked();
    for (uint32_t i = 0; i < props->Length(); ++i) {
        SWIGV8_VALUE key_local = props->Get(SWIGV8_CURRENT_CONTEXT(), i).ToLocalChecked();
        auto utf8Value = String::Utf8Value(Isolate::GetCurrent(), key_local);
        SWIGV8_OBJECT buffer = cbAssocArray->Get(SWIGV8_CURRENT_CONTEXT(), key_local)
                                   .ToLocalChecked()
                                   ->ToObject(SWIGV8_CURRENT_CONTEXT())
                                   .ToLocalChecked();
        Local<Function> func = Local<Function>::Cast(buffer);
        setCallback(toView(utf8Value), func);
    }
}

void
handlePendingSignals(uv_async_t* async_data)
{
    SWIGV8_HANDLESCOPE();
    std::lock_guard lock(pendingSignalsLock);
    while (not pendingSignals.empty()) {
        pendingSignals.front()();
        pendingSignals.pop();
    }
}

void
registrationStateChanged(const std::string& accountId,
                         const std::string& state,
                         int code,
                         const std::string& detail_str)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, code, detail_str]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    registrationStateChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(state),
                                            SWIGV8_INTEGER_NEW(code),
                                            V8_STRING_NEW_LOCAL(detail_str)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
composingStatusChanged(const std::string& accountId,
                       const std::string& conversationId,
                       const std::string& from,
                       int state)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, from, state]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), composingStatusChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            V8_STRING_NEW_LOCAL(from),
                                            SWIGV8_INTEGER_NEW(state)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
volatileDetailsChanged(const std::string& accountId,
                       const std::map<std::string, std::string>& details)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, details]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), volatileDetailsChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            stringMapToJsMap(details)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
accountDetailsChanged(const std::string& accountId,
                      const std::map<std::string, std::string>& details)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, details]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), accountDetailsChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            stringMapToJsMap(details)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
accountsChanged()
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), accountsChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 0, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
contactAdded(const std::string& accountId, const std::string& uri, bool confirmed)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, uri, confirmed]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), contactAddedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(uri),
                                            SWIGV8_BOOLEAN_NEW(confirmed)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
contactRemoved(const std::string& accountId, const std::string& uri, bool banned)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, uri, banned]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), contactRemovedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(uri),
                                            SWIGV8_BOOLEAN_NEW(banned)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
nameRegistrationEnded(const std::string& accountId, int state, const std::string& name)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, name]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), nameRegistrationEndedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            SWIGV8_INTEGER_NEW(state),
                                            V8_STRING_NEW_LOCAL(name)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
registeredNameFound(const std::string& accountId,
                    const std::string& requestName,
                    int state,
                    const std::string& address,
                    const std::string& name)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, requestName, state, address, name]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), registeredNameFoundCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(requestName),
                                            SWIGV8_INTEGER_NEW(state),
                                            V8_STRING_NEW_LOCAL(address),
                                            V8_STRING_NEW_LOCAL(name)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 5, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
accountMessageStatusChanged(const std::string& account_id,
                            const std::string& conversationId,
                            const std::string& peer,
                            const std::string& message_id,
                            int state)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([account_id, message_id, peer, state]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    accountMessageStatusChangedCb);
        if (!func.IsEmpty()) {
            Local<Value> callback_args[] = {V8_STRING_NEW_LOCAL(account_id),
                                            V8_STRING_NEW_LOCAL(message_id),
                                            V8_STRING_NEW_LOCAL(peer),
                                            SWIGV8_INTEGER_NEW(state)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
needsHost(const std::string& account_id, const std::string& conversationId)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([account_id, conversationId]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), needsHostCb);
        if (!func.IsEmpty()) {
            Local<Value> callback_args[] = {V8_STRING_NEW_LOCAL(account_id),
                                            V8_STRING_NEW_LOCAL(conversationId)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
activeCallsChanged(const std::string& account_id,
                   const std::string& conversationId,
                   const std::vector<std::map<std::string, std::string>>& activeCalls)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([account_id, conversationId, activeCalls]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), activeCallsChangedCb);
        if (!func.IsEmpty()) {
            Local<Value> callback_args[] = {V8_STRING_NEW_LOCAL(account_id),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            stringMapVecToJsMapArray(activeCalls)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
incomingAccountMessage(const std::string& accountId,
                       const std::string& messageId,
                       const std::string& from,
                       const std::map<std::string, std::string>& payloads)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, from, payloads]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingAccountMessageCb);
        if (!func.IsEmpty()) {
            SWIGV8_OBJECT jsMap = stringMapToJsMap(payloads);
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(from),
                                            jsMap};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
knownDevicesChanged(const std::string& accountId, const std::map<std::string, std::string>& devices)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, devices]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), knownDevicesChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_OBJECT jsMap = stringMapToJsMap(devices);
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId), jsMap};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
userSearchEnded(const std::string& accountId,
                int state,
                const std::string& query,
                const std::vector<std::map<std::string, std::string>>& results)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, query, results]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), userSearchEndedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            SWIGV8_INTEGER_NEW(state),
                                            V8_STRING_NEW_LOCAL(query),
                                            stringMapVecToJsMapArray(results)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
deviceRevocationEnded(const std::string& accountId, const std::string& device, int status)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, device, status]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), deviceRevocationEndedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(device),
                                            SWIGV8_INTEGER_NEW(status)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
deviceAuthStateChanged(const std::string& accountId,
                       int state,
                       const std::map<std::string, std::string>& details)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, details]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), deviceAuthStateChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_OBJECT jsMap = stringMapToJsMap(details);
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            SWIGV8_INTEGER_NEW(state),
                                            jsMap};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
addDeviceStateChanged(const std::string& accountId,
                      uint32_t opId,
                      int state,
                      const std::map<std::string, std::string>& details)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, opId, state, details]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), addDeviceStateChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_OBJECT jsMap = stringMapToJsMap(details);
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            SWIGV8_INTEGER_NEW_UNS(opId),
                                            SWIGV8_INTEGER_NEW(state),
                                            jsMap};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
incomingTrustRequest(const std::string& accountId,
                     const std::string& from,
                     const std::vector<uint8_t>& payload,
                     time_t received)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, from, payload, received]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingTrustRequestCb);
        if (!func.IsEmpty()) {
            SWIGV8_ARRAY jsArray = intVectToJsArray(payload);
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(from),
                                            jsArray,
                                            SWIGV8_NUMBER_NEW(received)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
callStateChanged(const std::string& accountId,
                 const std::string& callId,
                 const std::string& state,
                 int detail_code)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, callId, state, detail_code]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), callStateChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(callId),
                                            V8_STRING_NEW_LOCAL(state),
                                            SWIGV8_INTEGER_NEW(detail_code)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
mediaChangeRequested(const std::string& accountId,
                     const std::string& callId,
                     const std::vector<std::map<std::string, std::string>>& mediaList)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, callId, mediaList]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), mediaChangeRequestedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(callId),
                                            stringMapVecToJsMapArray(mediaList)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
incomingMessage(const std::string& accountId,
                const std::string& id,
                const std::string& from,
                const std::map<std::string, std::string>& messages)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, id, from, messages]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingMessageCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(id),
                                            V8_STRING_NEW_LOCAL(from),
                                            stringMapToJsMap(messages)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
incomingCall(const std::string& accountId, const std::string& callId, const std::string& from)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, callId, from]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingCallCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(callId),
                                            V8_STRING_NEW_LOCAL(from)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
incomingCallWithMedia(const std::string& accountId,
                      const std::string& callId,
                      const std::string& from,
                      const std::vector<std::map<std::string, std::string>>& mediaList)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, callId, from, mediaList]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingCallWithMediaCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(callId),
                                            V8_STRING_NEW_LOCAL(from),
                                            stringMapVecToJsMapArray(mediaList)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

/** Data Transfer */

void
dataTransferEvent(const std::string& accountId,
                  const std::string& conversationId,
                  const std::string& interactionId,
                  const std::string& fileId,
                  int eventCode)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, interactionId, fileId, eventCode]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), dataTransferEventCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            V8_STRING_NEW_LOCAL(interactionId),
                                            V8_STRING_NEW_LOCAL(fileId),
                                            SWIGV8_INTEGER_NEW(eventCode)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 5, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

/** Conversations */

void
conversationLoaded(uint32_t id,
                   const std::string& accountId,
                   const std::string& conversationId,
                   const std::vector<std::map<std::string, std::string>>& message)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([id, accountId, conversationId, message]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conversationLoadedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {SWIGV8_INTEGER_NEW_UNS(id),
                                            V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            stringMapVecToJsMapArray(message)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
swarmLoaded(uint32_t id,
            const std::string& accountId,
            const std::string& conversationId,
            const std::vector<libjami::SwarmMessage>& messages)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([id, accountId, conversationId, messages]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), swarmLoadedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {SWIGV8_INTEGER_NEW_UNS(id),
                                            V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            swarmMessagesToJsArray(messages)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
swarmMessageReceived(const std::string& accountId,
                     const std::string& conversationId,
                     const libjami::SwarmMessage& message)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, message]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), swarmMessageReceivedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            swarmMessageToJs(message)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
swarmMessageUpdated(const std::string& accountId,
                    const std::string& conversationId,
                    const libjami::SwarmMessage& message)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, message]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), swarmMessageUpdatedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            swarmMessageToJs(message)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
reactionAdded(const std::string& accountId,
              const std::string& conversationId,
              const std::string& messageId,
              const std::map<std::string, std::string>& reaction)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, messageId, reaction]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), reactionAddedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            V8_STRING_NEW_LOCAL(messageId),
                                            stringMapToJsMap(reaction)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
reactionRemoved(const std::string& accountId,
                const std::string& conversationId,
                const std::string& messageId,
                const std::string& reactionId)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, messageId, reactionId]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), reactionRemovedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            V8_STRING_NEW_LOCAL(messageId),
                                            V8_STRING_NEW_LOCAL(reactionId)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
messagesFound(uint32_t id,
              const std::string& accountId,
              const std::string& conversationId,
              const std::vector<std::map<std::string, std::string>>& messages)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([id, accountId, conversationId, messages]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), messagesFoundCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {SWIGV8_INTEGER_NEW_UNS(id),
                                            V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            stringMapVecToJsMapArray(messages)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
messageReceived(const std::string& accountId,
                const std::string& conversationId,
                const std::map<std::string, std::string>& message)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, message]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), messageReceivedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            stringMapToJsMap(message)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conversationProfileUpdated(const std::string& accountId,
                           const std::string& conversationId,
                           const std::map<std::string, std::string>& profile)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, profile]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    conversationProfileUpdatedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            stringMapToJsMap(profile)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conversationRequestReceived(const std::string& accountId,
                            const std::string& conversationId,
                            const std::map<std::string, std::string>& message)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, message]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    conversationRequestReceivedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            stringMapToJsMap(message)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conversationRequestDeclined(const std::string& accountId, const std::string& conversationId)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    conversationRequestDeclinedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conversationReady(const std::string& accountId, const std::string& conversationId)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conversationReadyCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(conversationId),
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conversationRemoved(const std::string& accountId, const std::string& conversationId)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conversationRemovedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(conversationId),
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conversationMemberEvent(const std::string& accountId,
                        const std::string& conversationId,
                        const std::string& memberUri,
                        int event)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, memberUri, event]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    conversationMemberEventCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            V8_STRING_NEW_LOCAL(memberUri),
                                            SWIGV8_INTEGER_NEW(event)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
onConversationError(const std::string& accountId,
                    const std::string& conversationId,
                    uint32_t code,
                    const std::string& what)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, code, what]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), onConversationErrorCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            SWIGV8_INTEGER_NEW_UNS(code),
                                            V8_STRING_NEW_LOCAL(what)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conferenceCreated(const std::string& accountId,
                  const std::string& conversationId,
                  const std::string& confId)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, confId, conversationId]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conferenceCreatedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(conversationId),
                                            V8_STRING_NEW_LOCAL(confId)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conferenceChanged(const std::string& accountId, const std::string& confId, const std::string& state)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, confId, state]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conferenceChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(confId),
                                            V8_STRING_NEW_LOCAL(state)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conferenceRemoved(const std::string& accountId, const std::string& confId)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, confId]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conferenceRemovedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(confId)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
onConferenceInfosUpdated(const std::string& accountId,
                         const std::string& confId,
                         const std::vector<std::map<std::string, std::string>>& infos)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, confId, infos]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    onConferenceInfosUpdatedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(confId),
                                            stringMapVecToJsMapArray(infos)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conversationPreferencesUpdated(const std::string& accountId,
                               const std::string& convId,
                               const std::map<std::string, std::string>& preferences)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, convId, preferences]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    conversationPreferencesUpdatedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(convId),
                                            stringMapToJsMap(preferences)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
logMessage(const std::string& message)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([message]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), messageSendCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(message)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 1, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
accountProfileReceived(const std::string& accountId,
                       const std::string& displayName,
                       const std::string& photo)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, displayName, photo]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), accountProfileReceivedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(displayName),
                                            V8_STRING_NEW_LOCAL(photo)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
profileReceived(const std::string& accountId, const std::string& from, const std::string& path)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, from, path]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), profileReceivedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(from),
                                            V8_STRING_NEW_LOCAL(path)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
subscriptionStateChanged(const std::string& accountId, const std::string& buddy_uri, int state)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, buddy_uri, state]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    subscriptionStateChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(buddy_uri),
                                            SWIGV8_INTEGER_NEW(state)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
nearbyPeerNotification(const std::string& accountId,
                       const std::string& buddy_uri,
                       int state,
                       const std::string& displayName)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, buddy_uri, state, displayName]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), nearbyPeerNotificationCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(buddy_uri),
                                            SWIGV8_INTEGER_NEW(state),
                                            V8_STRING_NEW_LOCAL(displayName)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
newBuddyNotification(const std::string& accountId,
                     const std::string& buddy_uri,
                     int status,
                     const std::string& line_status)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, buddy_uri, status, line_status]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), newBuddyNotificationCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(buddy_uri),
                                            SWIGV8_INTEGER_NEW(status),
                                            V8_STRING_NEW_LOCAL(line_status)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
newServerSubscriptionRequest(const std::string& remote)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([remote]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    newServerSubscriptionRequestCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(remote)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 1, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
serverError(const std::string& accountId, const std::string& error, const std::string& msg)
{
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, error, msg]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), serverErrorCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId),
                                            V8_STRING_NEW_LOCAL(error),
                                            V8_STRING_NEW_LOCAL(msg)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}
