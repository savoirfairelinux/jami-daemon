#pragma once

#define V8_STRING_NEW(str) v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), str.data(), v8::String::kNormalString, str.size())

using namespace v8;

Persistent<Function> accountsChangedCb;
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


Persistent<Function>* getPresistentCb(const std::string &signal){
    if (signal == "AccountsChanged")
        return &accountsChangedCb;
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
    else return nullptr;
}


void intVectToJsArray(const std::vector<uint8_t>& intVect, const Local<Array>& jsArray){
    for (unsigned int i = 0; i < intVect.size(); i++ )
      jsArray->Set(SWIGV8_INTEGER_NEW_UNS(i), SWIGV8_INTEGER_NEW(intVect[i]));
}

void stringMapToJsMap( const std::map<std::string, std::string>&  strmap, const Local<Object> &jsMap){
    for (auto& kvpair: strmap)
        jsMap->Set(V8_STRING_NEW(std::get<0>(kvpair)), V8_STRING_NEW(std::get<1>(kvpair)));
}
void setCallback(const std::string& signal, Local<Function>& func){
    if (auto* presistentCb = getPresistentCb(signal)) {
        if (func->IsObject() && func->IsFunction()) {
            presistentCb->Reset(Isolate::GetCurrent(), func);
        } else {
            presistentCb->Reset();
        }
    }
    else {
        printf("No Signal Associated with Event \'%s\'\n", signal.c_str() );
    }
}

void parseCbMap(const Local<Value>& callbackMap){
    Local<Object> cbAssocArray  = callbackMap->ToObject();
    Local<Array> props = cbAssocArray->GetOwnPropertyNames();
    for (uint32_t i = 0; i < props->Length(); ++i) {
        const Local<Value> key_local = props->Get(i);
        std::string key = *String::Utf8Value(key_local);
        Handle<Object> buffer = cbAssocArray->Get(V8_STRING_NEW(key))->ToObject();
        Local<Function> func = Local<Function>::Cast(buffer);
        setCallback(key, func);
    }
}

void registrationStateChanged(const std::string& account_id, const std::string& state, int code, const std::string& detail_str){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), registrationStateChangedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), V8_STRING_NEW(state),
                                         SWIGV8_INTEGER_NEW(code), V8_STRING_NEW(detail_str) };

        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 4, callback_args);
    }
}
void accountsChanged(){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), accountsChangedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 0, callback_args);
    }
}
void contactAdded(const std::string& account_id, const std::string& uri, bool confirmed){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), contactAddedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), V8_STRING_NEW(uri),
                                         SWIGV8_BOOLEAN_NEW(confirmed)};
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }

}
void contactRemoved(const std::string& account_id, const std::string& uri, bool banned){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), contactRemovedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), V8_STRING_NEW(uri),
                                         SWIGV8_BOOLEAN_NEW(banned) };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }

}
void exportOnRingEnded(const std::string& account_id, int state, const std::string& pin){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), exportOnRingEndedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), SWIGV8_INTEGER_NEW(state),
                                         V8_STRING_NEW(pin) };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }

}
void nameRegistrationEnded(const std::string& account_id, int state, const std::string& name){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), nameRegistrationEndedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), SWIGV8_INTEGER_NEW(state),
                                         V8_STRING_NEW(name)};
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }

}
void registeredNameFound(const std::string& account_id, int state, const std::string& address, const std::string& name){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), registeredNameFoundCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), SWIGV8_INTEGER_NEW(state),
                                         V8_STRING_NEW(address), V8_STRING_NEW(name) };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 4, callback_args);
    }

}
void volatileDetailsChanged(const std::string& account_id, const std::map<std::string, std::string>&  details ){
    SWIGV8_HANDLESCOPE();
    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), volatileDetailsChangedCb);
    if (!func.IsEmpty()) {
        Local<Object> jsMap = SWIGV8_OBJECT_NEW();
        stringMapToJsMap(details, jsMap);
        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), jsMap};
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 2, callback_args);
    }
}


void accountMessageStatusChanged(const std::string& account_id, uint64_t message_id, const std::string& to, int state) {
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), accountMessageStatusChangedCb);
    if (!func.IsEmpty()){

        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), SWIGV8_INTEGER_NEW_UNS(message_id),
                                         V8_STRING_NEW(to), SWIGV8_INTEGER_NEW(state) };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 4, callback_args);
    }

}
void incomingAccountMessage(const std::string& account_id, const std::string& from, const std::map<std::string, std::string>& payloads){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingAccountMessageCb);
    if (!func.IsEmpty()) {
        Local<Object> jsMap = SWIGV8_OBJECT_NEW();
        stringMapToJsMap(payloads, jsMap);
        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), V8_STRING_NEW(from), jsMap};
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }
}
void knownDevicesChanged(const std::string& account_id, const std::map<std::string, std::string>& devices){
    SWIGV8_HANDLESCOPE();
    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), knownDevicesChangedCb);
    if (!func.IsEmpty()) {
        Local<Object> jsMap = SWIGV8_OBJECT_NEW();
        stringMapToJsMap(devices, jsMap);
        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), jsMap};
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 2, callback_args);
    }
}

void incomingTrustRequest(const std::string& account_id, const std::string& from, const std::vector<uint8_t>& payload, time_t received){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingTrustRequestCb);
    if (!func.IsEmpty()) {

        Local<Array> jsArray = SWIGV8_ARRAY_NEW();
        intVectToJsArray(payload, jsArray);

        Local<Value> callback_args[] = { V8_STRING_NEW(account_id), V8_STRING_NEW(from),
                                         jsArray, SWIGV8_NUMBER_NEW(received)};
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 4, callback_args);
    }

}
