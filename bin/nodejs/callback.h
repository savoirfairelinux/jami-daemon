
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


Persistent<Function>* getPresistentCb(std::string signal){
    if(strcmp(signal.c_str(),"AccountsChanged"))
        return &accountsChangedCb;
    else if(strcmp(signal.c_str(),"RegistrationStateChanged"))
        return &registrationStateChangedCb;
    else if(strcmp(signal.c_str(),"VolatileDetailsChanged"))
        return &volatileDetailsChangedCb;
    else if(strcmp(signal.c_str(),"IncomingAccountMessage"))
        return &incomingAccountMessageCb;
    else if(strcmp(signal.c_str(),"AccountMessageStatusChanged"))
        return &accountMessageStatusChangedCb;
    else if(strcmp(signal.c_str(),"IncomingTrustRequest"))
        return &incomingTrustRequestCb;
    else if(strcmp(signal.c_str(),"ContactAdded"))
        return &contactAddedCb;
    else if(strcmp(signal.c_str(),"ContactRemoved"))
        return &contactRemovedCb;
    else if(strcmp(signal.c_str(),"ExportOnRingEnded"))
        return &exportOnRingEndedCb;
    else if(strcmp(signal.c_str(),"NameRegistrationEnded"))
        return &nameRegistrationEndedCb;
    else if(strcmp(signal.c_str(),"KnownDevicesChanged"))
        return &knownDevicesChangedCb;
    else if(strcmp(signal.c_str(),"RegisteredNameFound"))
        return &registeredNameFoundCb;
    else return NULL;
}

void setCallback(const std::string &signal, Local<Function> &func){
    auto *presistentCb = getPresistentCb(signal);
    if(presistentCb != NULL){
        if (func->IsObject() && func->IsFunction()) {
            presistentCb->Reset(Isolate::GetCurrent(), func);
        } else {
            presistentCb->Reset();
        }
    }
    else {
        printf("No Signal Associated with Event \'%s\'\n",signal.c_str() );
    }
}

void parseCbMap(const Local<Value> &arg){
    Local<Object>  array  = arg->ToObject();
    Local<Array> props = array->GetOwnPropertyNames();
    for (uint32_t i = 0; i < props->Length(); ++i) {
        const Local<Value> key_local = props->Get(i);
        std::string key = *String::Utf8Value(key_local);
        Handle<Object> buffer = array->Get(SWIGV8_STRING_NEW(key.c_str()))->ToObject();
        Local<Function> func = Local<Function>::Cast(buffer);
        setCallback(key,func);
    }
}

void registrationStateChanged(const std::string& account_id,const std::string& state,int code,const std::string& detail_str){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), registrationStateChangedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { SWIGV8_STRING_NEW(account_id.c_str()), SWIGV8_STRING_NEW(state.c_str()),
                                                 SWIGV8_INTEGER_NEW(code),SWIGV8_STRING_NEW(detail_str.c_str()) };

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
        Local<Value> callback_args[] = {SWIGV8_STRING_NEW(account_id.c_str()),SWIGV8_STRING_NEW(uri.c_str()),
                                        SWIGV8_BOOLEAN_NEW(confirmed)};
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }

}
void contactRemoved(const std::string& account_id, const std::string& uri, bool banned){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), contactRemovedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = {SWIGV8_STRING_NEW(account_id.c_str()),SWIGV8_STRING_NEW(uri.c_str()),
                                        SWIGV8_BOOLEAN_NEW(banned) };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }

}
void exportOnRingEnded(const std::string& account_id, int state, const std::string& pin){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), exportOnRingEndedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = {SWIGV8_STRING_NEW(account_id.c_str()), SWIGV8_INTEGER_NEW(state),
                                        SWIGV8_STRING_NEW(pin.c_str()) };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }

}
void nameRegistrationEnded(const std::string& account_id, int state, const std::string& name){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), nameRegistrationEndedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { SWIGV8_STRING_NEW(account_id.c_str()), SWIGV8_INTEGER_NEW(state),
                                        SWIGV8_STRING_NEW(name.c_str())};
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }

}
void registeredNameFound(const std::string& account_id, int state, const std::string& address, const std::string& name){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), registeredNameFoundCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = {SWIGV8_STRING_NEW(account_id.c_str()),SWIGV8_INTEGER_NEW(state),
                                        SWIGV8_STRING_NEW(address.c_str()),SWIGV8_STRING_NEW(name.c_str()) };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 4, callback_args);
    }

}
/*
void accountMessageStatusChanged(const std::string& account_id, uint64_t message_id, const std::string& to, int state){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), accountMessageStatusChangedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { SWIGV8_STRING_NEW(account_id.c_str()), SWIGV8_INTEGER_NEW(message_id)
                                                SWIGV8_STRING_NEW(to.c_str()),SWIGV8_INTEGER_NEW(state) };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 4, callback_args);
    }

}
void incomingTrustRequest(const std::string& account_id, const std::string& from, const std::vector<uint8_t>& payload, time_t received){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingTrustRequestCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 4, callback_args);
    }

}
void knownDevicesChanged(const std::string& account_id, const std::map<std::string, std::string>& devices){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), knownDevicesChangedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }

}*/
/*void volatileDetailsChanged(const std::string& account_id, const std::map<std::string, std::string>&  details ){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), volatileDetailsChangedCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 3, callback_args);
    }
}
void incomingAccountMessage(const std::string& account_id, const std::string& from, const std::map<std::string, std::string>& payloads){
    SWIGV8_HANDLESCOPE();

    Local<Function> func = Local<Function>::New(Isolate::GetCurrent(), incomingAccountMessageCb);
    if (!func.IsEmpty()) {
        Local<Value> callback_args[] = { };
        func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 4, callback_args);
    }
}*/