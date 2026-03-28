#pragma once

#include <napi.h>
#include <uv.h>

#include <queue>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <unordered_map>

static napi_env g_env = nullptr;

struct StringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
};
static std::unordered_map<std::string, napi_ref, StringHash, std::equal_to<>> callbackRefs;

std::queue<std::function<void()>> pendingSignals;
std::mutex pendingSignalsLock;

uv_async_t signalAsync;

// N-API value creation helpers

inline napi_value napiString(const std::string& str) {
    napi_value result;
    napi_create_string_utf8(g_env, str.data(), str.size(), &result);
    return result;
}

inline napi_value napiInt32(int32_t val) {
    napi_value result;
    napi_create_int32(g_env, val, &result);
    return result;
}

inline napi_value napiUint32(uint32_t val) {
    napi_value result;
    napi_create_uint32(g_env, val, &result);
    return result;
}

inline napi_value napiBool(bool val) {
    napi_value result;
    napi_get_boolean(g_env, val, &result);
    return result;
}

inline napi_value napiDouble(double val) {
    napi_value result;
    napi_create_double(g_env, val, &result);
    return result;
}

inline napi_value napiObject() {
    napi_value result;
    napi_create_object(g_env, &result);
    return result;
}

inline napi_value napiArray(size_t len) {
    napi_value result;
    napi_create_array_with_length(g_env, len, &result);
    return result;
}

inline void napiSetElement(napi_value arr, uint32_t index, napi_value val) {
    napi_set_element(g_env, arr, index, val);
}

inline void napiSetProperty(napi_value obj, const char* key, napi_value val) {
    napi_set_named_property(g_env, obj, key, val);
}

inline void napiSetProperty(napi_value obj, const std::string& key, napi_value val) {
    napi_set_named_property(g_env, obj, key.c_str(), val);
}

// Type conversion helpers

inline napi_value intVectToJsArray(const std::vector<uint8_t>& intVect) {
    napi_value arr = napiArray(intVect.size());
    for (uint32_t i = 0; i < intVect.size(); i++)
        napiSetElement(arr, i, napiInt32(intVect[i]));
    return arr;
}

inline napi_value stringMapToJsMap(const std::map<std::string, std::string>& strmap) {
    napi_value obj = napiObject();
    for (const auto& [key, value] : strmap)
        napiSetProperty(obj, key, napiString(value));
    return obj;
}

inline napi_value stringIntMapToJsMap(const std::map<std::string, int32_t>& strmap) {
    napi_value obj = napiObject();
    for (const auto& [key, value] : strmap)
        napiSetProperty(obj, key, napiInt32(value));
    return obj;
}

inline napi_value stringMapVecToJsMapArray(const std::vector<std::map<std::string, std::string>>& vect) {
    napi_value arr = napiArray(vect.size());
    for (uint32_t i = 0; i < vect.size(); i++)
        napiSetElement(arr, i, stringMapToJsMap(vect[i]));
    return arr;
}

inline napi_value swarmMessageToJs(const libjami::SwarmMessage& message) {
    napi_value obj = napiObject();
    napiSetProperty(obj, "id", napiString(message.id));
    napiSetProperty(obj, "type", napiString(message.type));
    napiSetProperty(obj, "linearizedParent", napiString(message.linearizedParent));
    napiSetProperty(obj, "body", stringMapToJsMap(message.body));
    napiSetProperty(obj, "reactions", stringMapVecToJsMapArray(message.reactions));
    napiSetProperty(obj, "editions", stringMapVecToJsMapArray(message.editions));
    napiSetProperty(obj, "status", stringIntMapToJsMap(message.status));
    return obj;
}

inline napi_value swarmMessagesToJsArray(const std::vector<libjami::SwarmMessage>& messages) {
    napi_value arr = napiArray(messages.size());
    for (uint32_t i = 0; i < messages.size(); i++)
        napiSetElement(arr, i, swarmMessageToJs(messages[i]));
    return arr;
}

// Callback management

void setCallback(const std::string& signal, napi_value func) {
    auto it = callbackRefs.find(signal);
    if (it != callbackRefs.end() && it->second) {
        napi_delete_reference(g_env, it->second);
        callbackRefs.erase(it);
    }

    napi_valuetype type;
    napi_typeof(g_env, func, &type);
    if (type == napi_function) {
        napi_ref ref;
        napi_create_reference(g_env, func, 1, &ref);
        callbackRefs[signal] = ref;
    }
}

void parseCbMap(napi_env env, napi_value callbackMap) {
    napi_value propNames;
    napi_get_property_names(env, callbackMap, &propNames);

    uint32_t length;
    napi_get_array_length(env, propNames, &length);

    for (uint32_t i = 0; i < length; i++) {
        napi_value key;
        napi_get_element(env, propNames, i, &key);

        size_t keyLen;
        napi_get_value_string_utf8(env, key, nullptr, 0, &keyLen);
        std::string keyStr(keyLen, '\0');
        napi_get_value_string_utf8(env, key, keyStr.data(), keyLen + 1, &keyLen);

        napi_value value;
        napi_get_property(env, callbackMap, key, &value);

        setCallback(keyStr, value);
    }
}

void callCallback(std::string_view signal, size_t argc, const napi_value* argv) {
    auto it = callbackRefs.find(signal);
    if (it == callbackRefs.end() || !it->second)
        return;

    napi_value func;
    napi_get_reference_value(g_env, it->second, &func);

    napi_valuetype type;
    napi_typeof(g_env, func, &type);
    if (type != napi_function)
        return;

    napi_value global;
    napi_get_global(g_env, &global);
    napi_call_function(g_env, global, func, argc, argv, nullptr);
}

void handlePendingSignals(uv_async_t*) {
    napi_handle_scope scope;
    napi_open_handle_scope(g_env, &scope);

    std::lock_guard lock(pendingSignalsLock);
    while (!pendingSignals.empty()) {
        pendingSignals.front()();
        pendingSignals.pop();
    }

    napi_close_handle_scope(g_env, scope);
}

// Configuration signals

void accountsChanged() {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([]() {
        callCallback("AccountsChanged", 0, nullptr);
    });
    uv_async_send(&signalAsync);
}

void accountDetailsChanged(const std::string& accountId,
                           const std::map<std::string, std::string>& details) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, details]() {
        napi_value args[] = {napiString(accountId), stringMapToJsMap(details)};
        callCallback("AccountDetailsChanged", 2, args);
    });
    uv_async_send(&signalAsync);
}

void registrationStateChanged(const std::string& accountId,
                              const std::string& state,
                              int code,
                              const std::string& detail_str) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, code, detail_str]() {
        napi_value args[] = {napiString(accountId), napiString(state),
                             napiInt32(code), napiString(detail_str)};
        callCallback("RegistrationStateChanged", 4, args);
    });
    uv_async_send(&signalAsync);
}

void composingStatusChanged(const std::string& accountId,
                            const std::string& conversationId,
                            const std::string& from,
                            int state) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, from, state]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             napiString(from), napiInt32(state)};
        callCallback("ComposingStatusChanged", 4, args);
    });
    uv_async_send(&signalAsync);
}

void volatileDetailsChanged(const std::string& accountId,
                            const std::map<std::string, std::string>& details) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, details]() {
        napi_value args[] = {napiString(accountId), stringMapToJsMap(details)};
        callCallback("VolatileDetailsChanged", 2, args);
    });
    uv_async_send(&signalAsync);
}

void incomingAccountMessage(const std::string& accountId,
                            const std::string& messageId,
                            const std::string& from,
                            const std::map<std::string, std::string>& payloads) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, from, payloads]() {
        napi_value args[] = {napiString(accountId), napiString(from),
                             stringMapToJsMap(payloads)};
        callCallback("IncomingAccountMessage", 3, args);
    });
    uv_async_send(&signalAsync);
}

void accountMessageStatusChanged(const std::string& account_id,
                                 const std::string& conversationId,
                                 const std::string& peer,
                                 const std::string& message_id,
                                 int state) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([account_id, message_id, peer, state]() {
        napi_value args[] = {napiString(account_id), napiString(message_id),
                             napiString(peer), napiInt32(state)};
        callCallback("AccountMessageStatusChanged", 4, args);
    });
    uv_async_send(&signalAsync);
}

void needsHost(const std::string& account_id, const std::string& conversationId) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([account_id, conversationId]() {
        napi_value args[] = {napiString(account_id), napiString(conversationId)};
        callCallback("NeedsHost", 2, args);
    });
    uv_async_send(&signalAsync);
}

void activeCallsChanged(const std::string& account_id,
                        const std::string& conversationId,
                        const std::vector<std::map<std::string, std::string>>& activeCalls) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([account_id, conversationId, activeCalls]() {
        napi_value args[] = {napiString(account_id), napiString(conversationId),
                             stringMapVecToJsMapArray(activeCalls)};
        callCallback("ActiveCallsChanged", 3, args);
    });
    uv_async_send(&signalAsync);
}

void contactAdded(const std::string& accountId, const std::string& uri, bool confirmed) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, uri, confirmed]() {
        napi_value args[] = {napiString(accountId), napiString(uri), napiBool(confirmed)};
        callCallback("ContactAdded", 3, args);
    });
    uv_async_send(&signalAsync);
}

void contactRemoved(const std::string& accountId, const std::string& uri, bool banned) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, uri, banned]() {
        napi_value args[] = {napiString(accountId), napiString(uri), napiBool(banned)};
        callCallback("ContactRemoved", 3, args);
    });
    uv_async_send(&signalAsync);
}

void nameRegistrationEnded(const std::string& accountId, int state, const std::string& name) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, name]() {
        napi_value args[] = {napiString(accountId), napiInt32(state), napiString(name)};
        callCallback("NameRegistrationEnded", 3, args);
    });
    uv_async_send(&signalAsync);
}

void registeredNameFound(const std::string& accountId,
                         const std::string& requestName,
                         int state,
                         const std::string& address,
                         const std::string& name) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, requestName, state, address, name]() {
        napi_value args[] = {napiString(accountId), napiString(requestName),
                             napiInt32(state), napiString(address), napiString(name)};
        callCallback("RegisteredNameFound", 5, args);
    });
    uv_async_send(&signalAsync);
}

void knownDevicesChanged(const std::string& accountId,
                         const std::map<std::string, std::string>& devices) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, devices]() {
        napi_value args[] = {napiString(accountId), stringMapToJsMap(devices)};
        callCallback("KnownDevicesChanged", 2, args);
    });
    uv_async_send(&signalAsync);
}

void deviceAuthStateChanged(const std::string& accountId,
                            int state,
                            const std::map<std::string, std::string>& details) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, details]() {
        napi_value args[] = {napiString(accountId), napiInt32(state),
                             stringMapToJsMap(details)};
        callCallback("DeviceAuthStateChanged", 3, args);
    });
    uv_async_send(&signalAsync);
}

void addDeviceStateChanged(const std::string& accountId,
                           uint32_t opId,
                           int state,
                           const std::map<std::string, std::string>& details) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, opId, state, details]() {
        napi_value args[] = {napiString(accountId), napiUint32(opId),
                             napiInt32(state), stringMapToJsMap(details)};
        callCallback("AddDeviceStateChanged", 4, args);
    });
    uv_async_send(&signalAsync);
}

void incomingTrustRequest(const std::string& accountId,
                          const std::string& from,
                          const std::vector<uint8_t>& payload,
                          time_t received) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, from, payload, received]() {
        napi_value args[] = {napiString(accountId), napiString(from),
                             intVectToJsArray(payload), napiDouble(received)};
        callCallback("IncomingTrustRequest", 4, args);
    });
    uv_async_send(&signalAsync);
}

void userSearchEnded(const std::string& accountId,
                     int state,
                     const std::string& query,
                     const std::vector<std::map<std::string, std::string>>& results) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, state, query, results]() {
        napi_value args[] = {napiString(accountId), napiInt32(state),
                             napiString(query), stringMapVecToJsMapArray(results)};
        callCallback("UserSearchEnded", 4, args);
    });
    uv_async_send(&signalAsync);
}

void deviceRevocationEnded(const std::string& accountId,
                           const std::string& device,
                           int status) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, device, status]() {
        napi_value args[] = {napiString(accountId), napiString(device),
                             napiInt32(status)};
        callCallback("DeviceRevocationEnded", 3, args);
    });
    uv_async_send(&signalAsync);
}

void logMessage(const std::string& message) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([message]() {
        napi_value args[] = {napiString(message)};
        callCallback("LogMessage", 1, args);
    });
    uv_async_send(&signalAsync);
}

void accountProfileReceived(const std::string& accountId,
                            const std::string& displayName,
                            const std::string& photo) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, displayName, photo]() {
        napi_value args[] = {napiString(accountId), napiString(displayName),
                             napiString(photo)};
        callCallback("AccountProfileReceived", 3, args);
    });
    uv_async_send(&signalAsync);
}

void profileReceived(const std::string& accountId,
                     const std::string& from,
                     const std::string& path) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, from, path]() {
        napi_value args[] = {napiString(accountId), napiString(from),
                             napiString(path)};
        callCallback("ProfileReceived", 3, args);
    });
    uv_async_send(&signalAsync);
}

// Call signals

void callStateChanged(const std::string& accountId,
                      const std::string& callId,
                      const std::string& state,
                      int detail_code) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, callId, state, detail_code]() {
        napi_value args[] = {napiString(accountId), napiString(callId),
                             napiString(state), napiInt32(detail_code)};
        callCallback("CallStateChanged", 4, args);
    });
    uv_async_send(&signalAsync);
}

void mediaChangeRequested(const std::string& accountId,
                          const std::string& callId,
                          const std::vector<std::map<std::string, std::string>>& mediaList) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, callId, mediaList]() {
        napi_value args[] = {napiString(accountId), napiString(callId),
                             stringMapVecToJsMapArray(mediaList)};
        callCallback("MediaChangeRequested", 3, args);
    });
    uv_async_send(&signalAsync);
}

void incomingMessage(const std::string& accountId,
                     const std::string& id,
                     const std::string& from,
                     const std::map<std::string, std::string>& messages) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, id, from, messages]() {
        napi_value args[] = {napiString(accountId), napiString(id),
                             napiString(from), stringMapToJsMap(messages)};
        callCallback("IncomingMessage", 4, args);
    });
    uv_async_send(&signalAsync);
}

void incomingCall(const std::string& accountId,
                  const std::string& callId,
                  const std::string& from,
                  const std::vector<std::map<std::string, std::string>>& mediaList) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, callId, from, mediaList]() {
        napi_value args[] = {napiString(accountId), napiString(callId),
                             napiString(from), stringMapVecToJsMapArray(mediaList)};
        callCallback("IncomingCall", 4, args);
    });
    uv_async_send(&signalAsync);
}

// Data transfer signals

void dataTransferEvent(const std::string& accountId,
                       const std::string& conversationId,
                       const std::string& interactionId,
                       const std::string& fileId,
                       int eventCode) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, interactionId, fileId, eventCode]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             napiString(interactionId), napiString(fileId),
                             napiInt32(eventCode)};
        callCallback("DataTransferEvent", 5, args);
    });
    uv_async_send(&signalAsync);
}

// Conversation signals

void swarmLoaded(uint32_t id,
                 const std::string& accountId,
                 const std::string& conversationId,
                 const std::vector<libjami::SwarmMessage>& messages) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([id, accountId, conversationId, messages]() {
        napi_value args[] = {napiUint32(id), napiString(accountId),
                             napiString(conversationId), swarmMessagesToJsArray(messages)};
        callCallback("SwarmLoaded", 4, args);
    });
    uv_async_send(&signalAsync);
}

void swarmMessageReceived(const std::string& accountId,
                          const std::string& conversationId,
                          const libjami::SwarmMessage& message) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, message]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             swarmMessageToJs(message)};
        callCallback("SwarmMessageReceived", 3, args);
    });
    uv_async_send(&signalAsync);
}

void swarmMessageUpdated(const std::string& accountId,
                         const std::string& conversationId,
                         const libjami::SwarmMessage& message) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, message]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             swarmMessageToJs(message)};
        callCallback("SwarmMessageUpdated", 3, args);
    });
    uv_async_send(&signalAsync);
}

void reactionAdded(const std::string& accountId,
                   const std::string& conversationId,
                   const std::string& messageId,
                   const std::map<std::string, std::string>& reaction) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, messageId, reaction]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             napiString(messageId), stringMapToJsMap(reaction)};
        callCallback("ReactionAdded", 4, args);
    });
    uv_async_send(&signalAsync);
}

void reactionRemoved(const std::string& accountId,
                     const std::string& conversationId,
                     const std::string& messageId,
                     const std::string& reactionId) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, messageId, reactionId]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             napiString(messageId), napiString(reactionId)};
        callCallback("ReactionRemoved", 4, args);
    });
    uv_async_send(&signalAsync);
}

void messagesFound(uint32_t id,
                   const std::string& accountId,
                   const std::string& conversationId,
                   const std::vector<std::map<std::string, std::string>>& messages) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([id, accountId, conversationId, messages]() {
        napi_value args[] = {napiUint32(id), napiString(accountId),
                             napiString(conversationId), stringMapVecToJsMapArray(messages)};
        callCallback("MessagesFound", 4, args);
    });
    uv_async_send(&signalAsync);
}

void conversationProfileUpdated(const std::string& accountId,
                                const std::string& conversationId,
                                const std::map<std::string, std::string>& profile) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, profile]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             stringMapToJsMap(profile)};
        callCallback("ConversationProfileUpdated", 3, args);
    });
    uv_async_send(&signalAsync);
}

void conversationRequestReceived(const std::string& accountId,
                                 const std::string& conversationId,
                                 const std::map<std::string, std::string>& message) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, message]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             stringMapToJsMap(message)};
        callCallback("ConversationRequestReceived", 3, args);
    });
    uv_async_send(&signalAsync);
}

void conversationRequestDeclined(const std::string& accountId,
                                 const std::string& conversationId) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId)};
        callCallback("ConversationRequestDeclined", 2, args);
    });
    uv_async_send(&signalAsync);
}

void conversationReady(const std::string& accountId,
                       const std::string& conversationId) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId)};
        callCallback("ConversationReady", 2, args);
    });
    uv_async_send(&signalAsync);
}

void conversationRemoved(const std::string& accountId,
                         const std::string& conversationId) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId)};
        callCallback("ConversationRemoved", 2, args);
    });
    uv_async_send(&signalAsync);
}

void conversationMemberEvent(const std::string& accountId,
                             const std::string& conversationId,
                             const std::string& memberUri,
                             int event) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, memberUri, event]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             napiString(memberUri), napiInt32(event)};
        callCallback("ConversationMemberEvent", 4, args);
    });
    uv_async_send(&signalAsync);
}

void onConversationError(const std::string& accountId,
                         const std::string& conversationId,
                         uint32_t code,
                         const std::string& what) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, conversationId, code, what]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             napiUint32(code), napiString(what)};
        callCallback("OnConversationError", 4, args);
    });
    uv_async_send(&signalAsync);
}

void conferenceCreated(const std::string& accountId,
                       const std::string& conversationId,
                       const std::string& confId) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, confId, conversationId]() {
        napi_value args[] = {napiString(accountId), napiString(conversationId),
                             napiString(confId)};
        callCallback("ConferenceCreated", 3, args);
    });
    uv_async_send(&signalAsync);
}

void conferenceChanged(const std::string& accountId,
                       const std::string& confId,
                       const std::string& state) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, confId, state]() {
        napi_value args[] = {napiString(accountId), napiString(confId),
                             napiString(state)};
        callCallback("ConferenceChanged", 3, args);
    });
    uv_async_send(&signalAsync);
}

void conferenceRemoved(const std::string& accountId,
                       const std::string& confId) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, confId]() {
        napi_value args[] = {napiString(accountId), napiString(confId)};
        callCallback("ConferenceRemoved", 2, args);
    });
    uv_async_send(&signalAsync);
}

void onConferenceInfosUpdated(const std::string& accountId,
                              const std::string& confId,
                              const std::vector<std::map<std::string, std::string>>& infos) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, confId, infos]() {
        napi_value args[] = {napiString(accountId), napiString(confId),
                             stringMapVecToJsMapArray(infos)};
        callCallback("OnConferenceInfosUpdated", 3, args);
    });
    uv_async_send(&signalAsync);
}

void conversationPreferencesUpdated(const std::string& accountId,
                                    const std::string& convId,
                                    const std::map<std::string, std::string>& preferences) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, convId, preferences]() {
        napi_value args[] = {napiString(accountId), napiString(convId),
                             stringMapToJsMap(preferences)};
        callCallback("ConversationPreferencesUpdated", 3, args);
    });
    uv_async_send(&signalAsync);
}

// Presence signals

void subscriptionStateChanged(const std::string& accountId,
                              const std::string& buddy_uri,
                              int state) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, buddy_uri, state]() {
        napi_value args[] = {napiString(accountId), napiString(buddy_uri),
                             napiInt32(state)};
        callCallback("SubscriptionStateChanged", 3, args);
    });
    uv_async_send(&signalAsync);
}

void nearbyPeerNotification(const std::string& accountId,
                            const std::string& buddy_uri,
                            int state,
                            const std::string& displayName) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, buddy_uri, state, displayName]() {
        napi_value args[] = {napiString(accountId), napiString(buddy_uri),
                             napiInt32(state), napiString(displayName)};
        callCallback("NearbyPeerNotification", 4, args);
    });
    uv_async_send(&signalAsync);
}

void newBuddyNotification(const std::string& accountId,
                          const std::string& buddy_uri,
                          int status,
                          const std::string& line_status) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, buddy_uri, status, line_status]() {
        napi_value args[] = {napiString(accountId), napiString(buddy_uri),
                             napiInt32(status), napiString(line_status)};
        callCallback("NewBuddyNotification", 4, args);
    });
    uv_async_send(&signalAsync);
}

void newServerSubscriptionRequest(const std::string& remote) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([remote]() {
        napi_value args[] = {napiString(remote)};
        callCallback("NewServerSubscriptionRequest", 1, args);
    });
    uv_async_send(&signalAsync);
}

void serverError(const std::string& accountId,
                 const std::string& error,
                 const std::string& msg) {
    std::lock_guard lock(pendingSignalsLock);
    pendingSignals.emplace([accountId, error, msg]() {
        napi_value args[] = {napiString(accountId), napiString(error),
                             napiString(msg)};
        callCallback("ServerError", 3, args);
    });
    uv_async_send(&signalAsync);
}

// Daemon initialization

void initJami(napi_env env, napi_value callbackMap) {
    g_env = env;
    parseCbMap(env, callbackMap);

    uv_loop_t* loop;
    napi_get_uv_event_loop(env, &loop);
    uv_async_init(loop, &signalAsync, handlePendingSignals);

    using namespace std::placeholders;
    using std::bind;
    using libjami::exportable_callback;
    using libjami::ConfigurationSignal;
    using libjami::CallSignal;
    using libjami::ConversationSignal;
    using libjami::PresenceSignal;
    using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;

    const std::map<std::string, SharedCallback> callEvHandlers = {
        exportable_callback<CallSignal::StateChange>(bind(&callStateChanged, _1, _2, _3, _4)),
        exportable_callback<CallSignal::IncomingMessage>(bind(&incomingMessage, _1, _2, _3, _4)),
        exportable_callback<CallSignal::IncomingCall>(bind(&incomingCall, _1, _2, _3, _4)),
        exportable_callback<CallSignal::MediaChangeRequested>(bind(&mediaChangeRequested, _1, _2, _3))
    };

    const std::map<std::string, SharedCallback> configEvHandlers = {
        exportable_callback<ConfigurationSignal::AccountsChanged>(bind(&accountsChanged)),
        exportable_callback<ConfigurationSignal::AccountDetailsChanged>(bind(&accountDetailsChanged, _1, _2)),
        exportable_callback<ConfigurationSignal::RegistrationStateChanged>(bind(&registrationStateChanged, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::ComposingStatusChanged>(bind(composingStatusChanged, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::ContactAdded>(bind(&contactAdded, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::ContactRemoved>(bind(&contactRemoved, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::NameRegistrationEnded>(bind(&nameRegistrationEnded, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::RegisteredNameFound>(bind(&registeredNameFound, _1, _2, _3, _4, _5)),
        exportable_callback<ConfigurationSignal::VolatileDetailsChanged>(bind(&volatileDetailsChanged, _1, _2)),
        exportable_callback<ConfigurationSignal::KnownDevicesChanged>(bind(&knownDevicesChanged, _1, _2)),
        exportable_callback<ConfigurationSignal::DeviceAuthStateChanged>(bind(&deviceAuthStateChanged, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::AddDeviceStateChanged>(bind(&addDeviceStateChanged, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::IncomingAccountMessage>(bind(&incomingAccountMessage, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::AccountMessageStatusChanged>(bind(&accountMessageStatusChanged, _1, _2, _3, _4, _5)),
        exportable_callback<ConfigurationSignal::MessageSend>(bind(&logMessage, _1)),
        exportable_callback<ConfigurationSignal::NeedsHost>(bind(&needsHost, _1, _2)),
        exportable_callback<ConfigurationSignal::ActiveCallsChanged>(bind(&activeCallsChanged, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::ProfileReceived>(bind(&profileReceived, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::AccountProfileReceived>(bind(&accountProfileReceived, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::UserSearchEnded>(bind(&userSearchEnded, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::DeviceRevocationEnded>(bind(&deviceRevocationEnded, _1, _2, _3)),
    };

    const std::map<std::string, SharedCallback> dataTransferEvHandlers = {
        exportable_callback<libjami::DataTransferSignal::DataTransferEvent>(bind(&dataTransferEvent, _1, _2, _3, _4, _5)),
    };

    const std::map<std::string, SharedCallback> conversationHandlers = {
        exportable_callback<ConversationSignal::SwarmLoaded>(bind(&swarmLoaded, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::SwarmMessageReceived>(bind(&swarmMessageReceived, _1, _2, _3)),
        exportable_callback<ConversationSignal::SwarmMessageUpdated>(bind(&swarmMessageUpdated, _1, _2, _3)),
        exportable_callback<ConversationSignal::ReactionAdded>(bind(&reactionAdded, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::ReactionRemoved>(bind(&reactionRemoved, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::ConversationProfileUpdated>(bind(&conversationProfileUpdated, _1, _2, _3)),
        exportable_callback<ConversationSignal::ConversationRequestReceived>(bind(&conversationRequestReceived, _1, _2, _3)),
        exportable_callback<ConversationSignal::ConversationRequestDeclined>(bind(&conversationRequestDeclined, _1, _2)),
        exportable_callback<ConversationSignal::ConversationReady>(bind(&conversationReady, _1, _2)),
        exportable_callback<ConversationSignal::ConversationRemoved>(bind(&conversationRemoved, _1, _2)),
        exportable_callback<ConversationSignal::ConversationMemberEvent>(bind(&conversationMemberEvent, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::OnConversationError>(bind(&onConversationError, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::ConversationPreferencesUpdated>(bind(&conversationPreferencesUpdated, _1, _2, _3))
    };

    const std::map<std::string, SharedCallback> presenceEvHandlers = {
        exportable_callback<PresenceSignal::NewServerSubscriptionRequest>(bind(&newServerSubscriptionRequest, _1)),
        exportable_callback<PresenceSignal::ServerError>(bind(&serverError, _1, _2, _3)),
        exportable_callback<PresenceSignal::NewBuddyNotification>(bind(&newBuddyNotification, _1, _2, _3, _4)),
        exportable_callback<PresenceSignal::NearbyPeerNotification>(bind(&nearbyPeerNotification, _1, _2, _3, _4)),
        exportable_callback<PresenceSignal::SubscriptionStateChanged>(bind(&subscriptionStateChanged, _1, _2, _3))
    };

    if (!libjami::init(static_cast<libjami::InitFlag>(libjami::LIBJAMI_FLAG_DEBUG)))
        return;

    registerSignalHandlers(configEvHandlers);
    registerSignalHandlers(callEvHandlers);
    registerSignalHandlers(conversationHandlers);
    registerSignalHandlers(dataTransferEvHandlers);
    registerSignalHandlers(presenceEvHandlers);
    libjami::start();
}
