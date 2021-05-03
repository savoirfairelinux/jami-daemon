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
Persistent<Function> volatileDetailsChangedCb;
Persistent<Function> incomingAccountMessageCb;
Persistent<Function> accountMessageStatusChangedCb;
Persistent<Function> incomingTrustRequestCb;
Persistent<Function> contactAddedCb;
Persistent<Function> contactRemovedCb;
Persistent<Function> exportOnRingEndedCb;
Persistent<Function> nameRegistrationEndedCb;
Persistent<Function> knownDevicesChangedCb;
Persistent<Function> registeredNameFoundCb;
Persistent<Function> callStateChangedCb;
Persistent<Function> incomingMessageCb;
Persistent<Function> incomingCallCb;
Persistent<Function> conversationLoadedCb;
Persistent<Function> messageReceivedCb;
Persistent<Function> conversationRequestReceivedCb;
Persistent<Function> conversationReadyCb;
Persistent<Function> conversationRemovedCb;
Persistent<Function> conversationMemberEventCb;
Persistent<Function> onConversationErrorCb;
Persistent<Function> conferenceCreatedCb;
Persistent<Function> conferenceChangedCb;
Persistent<Function> conferenceRemovedCb;
Persistent<Function> onConferenceInfosUpdatedCb;

std::queue<std::function<void() >> pendingSignals;
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
    else if (signal == "VolatileDetailsChanged")
        return &volatileDetailsChangedCb;
    else if (signal == "IncomingAccountMessage")
        return &incomingAccountMessageCb;
    else if (signal == "AccountMessageStatusChanged")
        return &accountMessageStatusChangedCb;
    else if (signal == "IncomingTrustRequest")
        return &incomingTrustRequestCb;
    else if (signal == "ContactAdded")
        return &contactAddedCb;
    else if (signal == "ContactRemoved")
        return &contactRemovedCb;
    else if (signal == "ExportOnRingEnded")
        return &exportOnRingEndedCb;
    else if (signal == "NameRegistrationEnded")
        return &nameRegistrationEndedCb;
    else if (signal == "KnownDevicesChanged")
        return &knownDevicesChangedCb;
    else if (signal == "RegisteredNameFound")
        return &registeredNameFoundCb;
    else if (signal == "CallStateChanged")
        return &callStateChangedCb;
    else if (signal == "IncomingMessage")
        return &incomingMessageCb;
    else if (signal == "IncomingCall")
        return &incomingCallCb;
    else if (signal == "ConversationLoaded")
        return &conversationLoadedCb;
    else if (signal == "MessageReceived")
        return &messageReceivedCb;
    else if (signal == "ConversationReady")
        return &conversationReadyCb;
    else if (signal == "ConversationRemoved")
        return &conversationRemovedCb;
    else if (signal == "ConversationRequestReceived")
        return &conversationRequestReceivedCb;
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

    else
        return nullptr;
}

#define V8_STRING_NEW(str) v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), str.data(), v8::NewStringType::kNormal, str.size())
#define V8_STRING_NEW_LOCAL(str) V8_STRING_NEW(str).ToLocalChecked()

inline std::string_view
toView(const String::Utf8Value& utf8)
{
    return {*utf8, utf8.length()};
}

inline SWIGV8_ARRAY
intVectToJsArray(const std::vector<uint8_t>& intVect)
{
    SWIGV8_ARRAY jsArray = SWIGV8_ARRAY_NEW(intVect.size());
    for (unsigned int i = 0; i < intVect.size(); i++)
        jsArray->Set(SWIGV8_CURRENT_CONTEXT(), SWIGV8_INTEGER_NEW_UNS(i), SWIGV8_INTEGER_NEW(intVect[i]));
    return jsArray;
}

inline SWIGV8_OBJECT
stringMapToJsMap(const std::map<std::string, std::string>& strmap)
{
    SWIGV8_OBJECT jsMap = SWIGV8_OBJECT_NEW();
    for (auto& kvpair : strmap)
        jsMap->Set(SWIGV8_CURRENT_CONTEXT(), V8_STRING_NEW_LOCAL(std::get<0>(kvpair)), V8_STRING_NEW_LOCAL(std::get<1>(kvpair)));
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
        printf("No Signal Associated with Event \'%.*s\'\n", (int)signal.size(), signal.data());
    }
}

void parseCbMap(const SWIGV8_VALUE& callbackMap) {
    SWIGV8_OBJECT cbAssocArray = callbackMap->ToObject(SWIGV8_CURRENT_CONTEXT()).ToLocalChecked();
    SWIGV8_ARRAY props = cbAssocArray->GetOwnPropertyNames(SWIGV8_CURRENT_CONTEXT()).ToLocalChecked();
    for (uint32_t i = 0; i < props->Length(); ++i) {
        SWIGV8_VALUE key_local = props->Get(SWIGV8_CURRENT_CONTEXT(), i).ToLocalChecked();
        auto utf8Value = String::Utf8Value(Isolate::GetCurrent(), key_local);
        SWIGV8_OBJECT buffer = cbAssocArray->Get(SWIGV8_CURRENT_CONTEXT(), key_local).ToLocalChecked()->ToObject(SWIGV8_CURRENT_CONTEXT()).ToLocalChecked();
        Local<Function> func = Local<Function>::Cast(buffer);
        setCallback(toView(utf8Value), func);
    }
}

void
handlePendingSignals(uv_async_t* async_data)
{
    SWIGV8_HANDLESCOPE();
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
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
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, code, detail_str]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(),
                                                    registrationStateChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(state),
                SWIGV8_INTEGER_NEW(code),
                V8_STRING_NEW_LOCAL(detail_str)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
volatileDetailsChanged(const std::string& accountId,
                       const std::map<std::string, std::string>& details)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, details]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), volatileDetailsChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                stringMapToJsMap(details)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void accountDetailsChanged(const std::string& accountId, const std::map<std::string, std::string>& details) {

    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, details]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), accountDetailsChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                stringMapToJsMap(details)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
accountsChanged()
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
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
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, uri, confirmed]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), contactAddedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId), V8_STRING_NEW_LOCAL(uri), SWIGV8_BOOLEAN_NEW(confirmed)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
contactRemoved(const std::string& accountId, const std::string& uri, bool banned)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, uri, banned]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), contactRemovedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId), V8_STRING_NEW_LOCAL(uri), SWIGV8_BOOLEAN_NEW(banned)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
exportOnRingEnded(const std::string& accountId, int state, const std::string& pin)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, pin]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), exportOnRingEndedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId), SWIGV8_INTEGER_NEW(state), V8_STRING_NEW_LOCAL(pin)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
nameRegistrationEnded(const std::string& accountId, int state, const std::string& name)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, name]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), nameRegistrationEndedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId), SWIGV8_INTEGER_NEW(state), V8_STRING_NEW_LOCAL(name)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
registeredNameFound(const std::string& accountId,
                    int state,
                    const std::string& address,
                    const std::string& name)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, address, name]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), registeredNameFoundCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId), SWIGV8_INTEGER_NEW(state), V8_STRING_NEW_LOCAL(address), V8_STRING_NEW_LOCAL(name)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void accountMessageStatusChanged(const std::string& account_id, uint64_t message_id, const std::string& to, int state) {

    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([account_id, message_id, to, state]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), accountMessageStatusChangedCb);
        if (!func.IsEmpty()) {
            Local<Value> callback_args[] = {V8_STRING_NEW_LOCAL(account_id), SWIGV8_INTEGER_NEW_UNS(message_id), V8_STRING_NEW_LOCAL(to), SWIGV8_INTEGER_NEW(state)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void incomingAccountMessage(const std::string& accountId, const std::string& messageId, const std::string& from, const std::map<std::string, std::string>& payloads) {

    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, from, payloads]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingAccountMessageCb);
        if (!func.IsEmpty()) {
            SWIGV8_OBJECT jsMap = stringMapToJsMap(payloads);
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(accountId), V8_STRING_NEW_LOCAL(from), jsMap};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
knownDevicesChanged(const std::string& accountId, const std::map<std::string, std::string>& devices)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
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
incomingTrustRequest(const std::string& accountId,
                     const std::string& from,
                     const std::vector<uint8_t>& payload,
                     time_t received)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, from, payload, received]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingTrustRequestCb);
        if (!func.IsEmpty()) {
            SWIGV8_ARRAY jsArray = intVectToJsArray(payload);
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(from), jsArray, SWIGV8_NUMBER_NEW(received)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
callStateChanged(const std::string& callId, const std::string& state, int detail_code)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([callId, state, detail_code]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), callStateChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {V8_STRING_NEW_LOCAL(callId), V8_STRING_NEW_LOCAL(state), SWIGV8_INTEGER_NEW(detail_code)};
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
incomingMessage(const std::string& id,
                const std::string& from,
                const std::map<std::string, std::string>& messages)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([id, from, messages]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingMessageCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(id),
                V8_STRING_NEW_LOCAL(from),
                stringMapToJsMap(messages)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

void
incomingCall(const std::string& accountId, const std::string& callId, const std::string& from)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, callId, from]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingCallCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(callId),
                V8_STRING_NEW_LOCAL(from)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });

    uv_async_send(&signalAsync);
}

/** Conversations */

void
conversationLoaded(uint32_t id, const std::string& accountId, const std::string& conversationId, const std::vector<std::map<std::string, std::string>>& message)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([id, accountId, conversationId, message]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conversationLoadedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                SWIGV8_INTEGER_NEW_UNS(id),
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(conversationId),
                stringMapVecToJsMapArray(message)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
messageReceived(const std::string& accountId, const std::string& conversationId, const std::map<std::string, std::string>& message)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, message]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), messageReceivedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(conversationId),
                stringMapToJsMap(message)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conversationRequestReceived(const std::string& accountId, const std::string& conversationId, const std::map<std::string, std::string>& message)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, message]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conversationRequestReceivedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(conversationId),
                stringMapToJsMap(message)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 3, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conversationReady(const std::string& accountId, const std::string& conversationId)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
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
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
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
conversationMemberEvent(const std::string& accountId, const std::string& conversationId, const std::string& memberUri, int event)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, memberUri, event]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conversationMemberEventCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(conversationId),
                V8_STRING_NEW_LOCAL(memberUri),
                SWIGV8_INTEGER_NEW(event)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
onConversationError(const std::string& accountId, const std::string& conversationId, uint32_t code, const std::string& what)
{
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, code, what]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), onConversationErrorCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(accountId),
                V8_STRING_NEW_LOCAL(conversationId),
                SWIGV8_INTEGER_NEW_UNS(code),
                V8_STRING_NEW_LOCAL(what)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 4, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conferenceCreated(const std::string& confId){
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([confId]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conferenceCreatedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(confId)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 1, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conferenceChanged(const std::string& confId, const std::string& state){
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([confId, state]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conferenceChangedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(confId),
                V8_STRING_NEW_LOCAL(state)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
conferenceRemoved(const std::string& confId){
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([confId]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), conferenceRemovedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(confId)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 1, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}

void
onConferenceInfosUpdated(const std::string& confId, const std::vector<std::map<std::string, std::string>>& infos) {
    std::lock_guard<std::mutex> lock(pendingSignalsLock);
    pendingSignals.emplace([confId, infos]() {
        Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), onConferenceInfosUpdatedCb);
        if (!func.IsEmpty()) {
            SWIGV8_VALUE callback_args[] = {
                V8_STRING_NEW_LOCAL(confId),
                stringMapVecToJsMapArray(infos)
            };
            func->Call(SWIGV8_CURRENT_CONTEXT(), SWIGV8_NULL(), 2, callback_args);
        }
    });
    uv_async_send(&signalAsync);
}
